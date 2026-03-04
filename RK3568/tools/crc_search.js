// CRC 算法暴力搜索脚本
// 5 组已知正确的设备帧向量（仅设备发出的帧）
//
// KEY_REMOVED 完整帧: 7E 6C 00 0F 00 00 05 00 11 00 00 00 00 44 A0
// KEY_PLACED  完整帧: 7E 6C 00 0F 00 00 05 00 11 01 00 00 00 D1 62
// NAK/SETCOM  完整帧: 7E 6C 01 00 FF 00 03 00 00 0F 03 3C 53
// ACK/SETCOM  完整帧: 7E 6C 01 00 FF 00 02 00 5A 0F A7 20
// Transmitted SET_COM (device ACKed in 場景A): 0F 01 -> 1C 11  (NOT used - may be from old code)

// 每个向量: { scope_name, bytes_for_crc, expected_crc }
// 我们对每帧测试多种 scope 假设

const frames = {
  KEY_REMOVED: { full: [0x7E,0x6C,0x00,0x0F,0x00,0x00,0x05,0x00,0x11,0x00,0x00,0x00,0x00], crc: 0x44A0 },
  KEY_PLACED:  { full: [0x7E,0x6C,0x00,0x0F,0x00,0x00,0x05,0x00,0x11,0x01,0x00,0x00,0x00], crc: 0xD162 },
  NAK_SETCOM:  { full: [0x7E,0x6C,0x01,0x00,0xFF,0x00,0x03,0x00,0x00,0x0F,0x03], crc: 0x3C53 },
  ACK_SETCOM:  { full: [0x7E,0x6C,0x01,0x00,0xFF,0x00,0x02,0x00,0x5A,0x0F], crc: 0xA720 },
};

// scope 切片函数: start=0表示从7E算起
// Frame 结构: [7E][6C][KeyId][FrameNo][Addr2 2B][Len 2B LE][Cmd][Data...]
// index:       0    1    2      3        4  5       6  7      8    9..
function scopes(frame) {
  const f = frame.full;
  return {
    'cmd+data':    f.slice(8),          // 当前假设
    'len+cmd+data':f.slice(6),          // Len(2B)+Cmd+Data
    'addr+len+cmd+data': f.slice(4),    // Addr2+Len+Cmd+Data
    'from_frameno':f.slice(3),          // FrameNo+Addr2+Len+Cmd+Data
    'from_keyid':  f.slice(2),          // KeyId+FrameNo+...
    'full_no_hdr': f.slice(2),          // same as from_keyid
    'no_magic':    f,                   // 全帧
  };
}

function buildTable(poly, ref) {
  const t = new Array(256);
  for (let i = 0; i < 256; i++) {
    let c = ref ? i : (i << 8);
    for (let j = 0; j < 8; j++) {
      if (ref) c = (c & 1) ? ((c >>> 1) ^ poly) : (c >>> 1);
      else     c = (c & 0x8000) ? (((c << 1) ^ poly) & 0xFFFF) : ((c << 1) & 0xFFFF);
    }
    t[i] = c & 0xFFFF;
  }
  return t;
}

function crc16tab(data, table, ref, init, xorOut) {
  let crc = init;
  for (const b of data) {
    if (ref) crc = ((crc >>> 8) ^ table[(crc ^ b) & 0xFF]) & 0xFFFF;
    else     crc = ((crc << 8) ^ table[((crc >>> 8) ^ b) & 0xFF]) & 0xFFFF;
  }
  return (crc ^ xorOut) & 0xFFFF;
}

// Step 1: 对所有 scope 假设，用 XMODEM(1021 noRef init=0 xor=0) 验证
console.log('=== Step1: XMODEM 各 scope 测试 ===');
const xmodemTab = buildTable(0x1021, false);
for (const [fname, frame] of Object.entries(frames)) {
  const sc = scopes(frame);
  for (const [sname, bytes] of Object.entries(sc)) {
    const got = crc16tab(bytes, xmodemTab, false, 0, 0);
    const ok = got === frame.crc ? '✓' : '✗';
    if (got === frame.crc) console.log(`${ok} ${fname} scope=${sname} got=0x${got.toString(16).padStart(4,'0')}`);
  }
}

// Step 2: 对每个 scope，暴力搜索 poly=1021(反射/非反射) + 所有 init(0..0xFFFF) + xor=0
console.log('\n=== Step2: poly=1021 brute init, all scopes ===');
const polyCandidates = [
  { poly: 0x1021, ref: false, name: 'CCITT-noRef' },
  { poly: 0x8408, ref: true,  name: 'CCITT-ref(KERMIT)' },
  { poly: 0x8005, ref: false, name: 'IBM-noRef' },
  { poly: 0xA001, ref: true,  name: 'IBM-ref(ARC)' },
];

// 搜索两帧就够验证了（避免速度太慢），命中后再全验证
const refVecs = [
  { d: frames.KEY_REMOVED, sc: null },
  { d: frames.KEY_PLACED,  sc: null },
  { d: frames.NAK_SETCOM,  sc: null },
  { d: frames.ACK_SETCOM,  sc: null },
];
const scopeNames = ['cmd+data','len+cmd+data','addr+len+cmd+data','from_frameno','from_keyid'];

let found = false;
for (const pc of polyCandidates) {
  const tab = buildTable(pc.poly, pc.ref);
  for (const sn of scopeNames) {
    // 只在 xor=0 的情况下暴力 init
    for (let init = 0; init <= 0xFFFF; init++) {
      let allOk = true;
      for (const [fname, frame] of Object.entries(frames)) {
        const sc = scopes(frame);
        const bytes = sc[sn];
        if (!bytes) { allOk = false; break; }
        if (crc16tab(bytes, tab, pc.ref, init, 0) !== frame.crc) { allOk = false; break; }
      }
      if (allOk) {
        found = true;
        console.log(`FOUND! poly=${pc.name} scope=${sn} init=0x${init.toString(16)} xor=0`);
      }
    }
    // 再试 xor=0xFFFF
    for (let init = 0; init <= 0xFFFF; init++) {
      let allOk = true;
      for (const [fname, frame] of Object.entries(frames)) {
        const sc = scopes(frame);
        const bytes = sc[sn];
        if (!bytes) { allOk = false; break; }
        if (crc16tab(bytes, tab, pc.ref, init, 0xFFFF) !== frame.crc) { allOk = false; break; }
      }
      if (allOk) {
        found = true;
        console.log(`FOUND! poly=${pc.name} scope=${sn} init=0x${init.toString(16)} xor=0xFFFF`);
      }
    }
  }
}

if (!found) {
  // Step 3: 尝试输入字节反转
  console.log('\n=== Step3: 尝试按字节反转输入 (nibble swap / bit reverse per byte) ===');
  function bitRevByte(b) {
    let r = 0;
    for (let i = 0; i < 8; i++) r = (r << 1) | ((b >> i) & 1);
    return r & 0xFF;
  }
  function nibbleSwap(b) { return ((b & 0x0F) << 4) | ((b >> 4) & 0x0F); }

  for (const transform of ['bitrev', 'nibble']) {
    const xmodemTab2 = buildTable(0x1021, false);
    for (const sn of scopeNames) {
      for (let init = 0; init <= 3; init++) { // 仅试 0,1,0xFFFF,0xFFFE
        const initVals = [0, 1, 0xFFFF, 0xFFFE];
        for (const initV of initVals) {
          let allOk = true;
          for (const [fname, frame] of Object.entries(frames)) {
            const sc2 = scopes(frame);
            const bytes = sc2[sn];
            if (!bytes) { allOk = false; break; }
            const transformed = bytes.map(b => transform === 'bitrev' ? bitRevByte(b) : nibbleSwap(b));
            if (crc16tab(transformed, xmodemTab2, false, initV, 0) !== frame.crc) { allOk = false; break; }
          }
          if (allOk) {
            console.log(`FOUND! transform=${transform} scope=${sn} init=0x${initV.toString(16)}`);
            found = true;
          }
        }
      }
    }
  }
}

if (!found) {
  console.log('\n=== Step4: 暴力全部65536 poly(noRef) + init=0 + scope=cmd+data ===');
  // 限定 init=0 xor=0 暴力搜索所有 poly
  for (let poly = 0; poly <= 0xFFFF; poly++) {
    const tab = buildTable(poly, false);
    let allOk = true;
    for (const [fname, frame] of Object.entries(frames)) {
      const bytes = frame.full.slice(8);
      if (crc16tab(bytes, tab, false, 0, 0) !== frame.crc) { allOk = false; break; }
    }
    if (allOk) {
      console.log(`FOUND poly=0x${poly.toString(16).padStart(4,'0')} noRef init=0 xor=0 scope=cmd+data`);
      found = true;
    }
  }
  if (!found) console.log('Step4: not found');
}

console.log('\nDone.');
