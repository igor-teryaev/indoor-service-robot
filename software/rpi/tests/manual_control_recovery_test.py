import os, pty, struct, time, signal, subprocess, threading, select, binascii, sys, tempfile

base = '/home/igor/build/indoor-service-robot/audit-20261023-probes/'
rx, rxs = pty.openpty()
stm, stms = pty.openpty()
wheels = []
heartbeats = []
sync = threading.Event()
done = threading.Event()
errors = []


def frame(typ, seq, payload):
    b = bytes([1, typ]) + struct.pack('>HH', seq, len(payload)) + payload
    return b'\xa5\x5a' + b + struct.pack('>H', binascii.crc_hqx(b, 0xffff))


def fake_stm32():
    buf = b''
    try:
        while not done.is_set():
            if not select.select([stm], [], [], .01)[0]: continue
            buf += os.read(stm, 4096)
            while len(buf) >= 8:
                if buf[:2] != b'\xa5\x5a': buf = buf[1:];continue
                n = struct.unpack('>H', buf[6:8])[0]
                total = 10 + n
                if len(buf) < total: break
                typ = buf[3]
                seq = struct.unpack('>H', buf[4:6])[0]
                p = buf[8:8 + n]
                buf = buf[total:]
                if typ == 1:
                    os.write(stm, frame(2, seq, p));sync.set()
                elif typ == 3:
                    heartbeats.append(time.monotonic())
                    os.write(stm, frame(3, seq, b'\x01' + bytes(4)))
                elif typ == 16:
                    os.write(stm, frame(17, seq, b'\0') + frame(18, seq, p + b'\0'))
                elif typ == 32:
                    wheels.append((time.monotonic(), struct.unpack('>Ihh', p)))
    except Exception as e:
        errors.append(repr(e))


def crsf(forward, sa):
    ch = [992] * 16
    ch[1] = forward
    ch[4] = sa
    bits = sum(v << (11 * i) for i, v in enumerate(ch))
    body = b'\x16' + bits.to_bytes(22, 'little')
    crc = 0
    for b in body:
        crc ^= b
        for _ in range(8): crc = ((crc << 1) ^ (0xd5 if crc & 128 else 0)) & 255
    return b'\xc8\x18' + body + bytes([crc])


def send(forward, sa): os.write(rx, crsf(forward, sa));time.sleep(.025)


log = tempfile.TemporaryFile(mode = 'w+t')
p = subprocess.Popen(
 [sys.argv[1], os.ttyname(rxs), os.ttyname(stms)],
 stdout=log, stderr=log)
t = threading.Thread(target=fake_stm32, daemon=True)
t.start()
try:
    assert sync.wait(2), 'no synchronization'
    time.sleep(.06)
    for _ in range(3): send(992, 191)
    for _ in range(3): send(992, 1792)
    for _ in range(6): send(1811, 1792)
    assert any(
        left != 0 or right != 0
        for _, (_, left, right) in wheels
    ), 'initial nonzero wheel command missing'
    os.kill(p.pid, signal.SIGSTOP)
    paused = time.monotonic()
    time.sleep(.06)
    os.write(rx, crsf(992, 191))
    time.sleep(.06)
    os.write(rx, crsf(992, 1792))
    time.sleep(.06)
    os.write(rx, crsf(1811, 1792))
    last_generated = time.monotonic()
    time.sleep(.42)
    resumed = time.monotonic()
    os.kill(p.pid, signal.SIGCONT)
    time.sleep(.18)
    assert any(t >= resumed for t in heartbeats), 'No STM32 heartbeat after process resume'
    assert p.poll() is None, 'manual_control_probe exited unexpectedly'
    assert not errors, f'Simulated STM32 failed: {errors}'
    replay = [w for w in wheels if w[0] >= resumed]
    print('Pause duration ms:', round((resumed - paused) * 1000))
    print('Last queued ELRS command age at resume ms:', round((resumed - last_generated) * 1000))
    print('Fresh ELRS frames sent after resume: 0')
    print('Nonzero wheel frames after resume:', len(replay))
    print('Replayed wheel values:', [w[1][1:] for w in replay])
    assert not any(
     left != 0 or right != 0
     for _, (_, left, right) in replay
    ), f"Stale buffered ELRS frames restarted motion: {replay}"
finally:
    if p.poll() is None:
        os.kill(p.pid, signal.SIGCONT)
        p.terminate()
    p.wait(timeout=2)
    done.set()
    t.join(1)
    log.close()
    for f in (rx, rxs, stm, stms): os.close(f)
    if errors: print('PTY errors:', errors)
