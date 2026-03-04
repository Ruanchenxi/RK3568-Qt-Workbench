# CRC-16 算法枚举测试脚本
# 用已知的设备真实帧验证哪种 CRC-16 变体与 X5 钥匙柜协议一致
#
# 已知真值（来自多次日志实测）：
#   11 00 00 00 00  →  44 A0  (KEY_REMOVED Cmd+Data)
#   11 01 00 00 00  →  D1 62  (KEY_PLACED  Cmd+Data)
#   00 0F 03        →  3C 53  (NAK for SET_COM Cmd+Data)
#   5A 0F           →  A7 20  (ACK for SET_COM Cmd+Data，来自 场景A.csv)
#   04 00           →  D1 4C  (Q_TASK resp count=0 Cmd+Data，来自 场景A.csv)
#   0F 01           →  ?      (我们 TX 的 SET_COM Cmd+Data，正确值待确认)
#
# 运行方法：
#   cd d:\Desktop\QTtest\RK3568\RK3568
#   .\tools\find_crc.ps1

function Build-Crc16Table {
    param([uint16]$poly, [bool]$reflected)
    $table = New-Object uint16[] 256
    for ($i = 0; $i -lt 256; $i++) {
        if ($reflected) {
            $crc = [uint16]$i
            for ($j = 0; $j -lt 8; $j++) {
                if ($crc -band 1) { $crc = [uint16](($crc -shr 1) -bxor $poly) }
                else              { $crc = [uint16]($crc -shr 1) }
            }
        } else {
            $crc = [uint16]($i -shl 8)
            for ($j = 0; $j -lt 8; $j++) {
                if ($crc -band 0x8000) { $crc = [uint16](($crc -shl 1) -bxor $poly) }
                else                   { $crc = [uint16]($crc -shl 1) }
            }
        }
        $table[$i] = $crc
    }
    return $table
}

function Calc-Crc16 {
    param([uint16[]]$table, [bool]$reflected, [uint16]$init, [uint16]$xorOut, [byte[]]$data)
    $crc = $init
    foreach ($b in $data) {
        if ($reflected) {
            $idx = ($crc -bxor $b) -band 0xFF
            $crc = [uint16](($crc -shr 8) -bxor $table[$idx])
        } else {
            $idx = (($crc -shr 8) -bxor $b) -band 0xFF
            $crc = [uint16](($crc -shl 8) -bxor $table[$idx])
        }
    }
    return [uint16]($crc -bxor $xorOut)
}

# 测试向量（Cmd+Data 字节数组 → 期望 CRC 大端 uint16）
$vectors = @(
    @{ hex = "11 00 00 00 00"; expect = 0x44A0; desc = "KEY_REMOVED" },
    @{ hex = "11 01 00 00 00"; expect = 0xD162; desc = "KEY_PLACED" },
    @{ hex = "00 0F 03";       expect = 0x3C53; desc = "NAK/SET_COM" },
    @{ hex = "5A 0F";          expect = 0xA720; desc = "ACK/SET_COM (场景A)" },
    @{ hex = "04 00";          expect = 0xD14C; desc = "Q_TASK resp=0 (场景A)" }
)

# 候选算法（名称、多项式、是否反射、初值、XOR输出）
$algos = @(
    @{ name = "CRC16/IBM       (poly=8005 ref  init=0000 xor=0000)"; poly=0x8005; ref=$true;  init=0x0000; xor=0x0000 },
    @{ name = "CRC16/MODBUS    (poly=8005 ref  init=FFFF xor=0000)"; poly=0x8005; ref=$true;  init=0xFFFF; xor=0x0000 },
    @{ name = "CRC16/XMODEM    (poly=1021 noref init=0000 xor=0000)"; poly=0x1021; ref=$false; init=0x0000; xor=0x0000 },
    @{ name = "CRC16/CCITT-FALSE(poly=1021 noref init=FFFF xor=0000)"; poly=0x1021; ref=$false; init=0xFFFF; xor=0x0000 },
    @{ name = "CRC16/KERMIT    (poly=1021 ref  init=0000 xor=0000)"; poly=0x1021; ref=$true;  init=0x0000; xor=0x0000 },
    @{ name = "CRC16/X.25      (poly=1021 ref  init=FFFF xor=FFFF)"; poly=0x1021; ref=$true;  init=0xFFFF; xor=0xFFFF },
    @{ name = "CRC16/BUYPASS   (poly=8005 noref init=0000 xor=0000)"; poly=0x8005; ref=$false; init=0x0000; xor=0x0000 },
    @{ name = "CRC16/ANSI      (poly=8005 noref init=FFFF xor=0000)"; poly=0x8005; ref=$false; init=0xFFFF; xor=0x0000 },
    @{ name = "CRC16/MCRF4XX   (poly=1021 ref  init=FFFF xor=0000)"; poly=0x1021; ref=$true;  init=0xFFFF; xor=0x0000 }
)

Write-Host "========== CRC-16 算法枚举测试 =========="
Write-Host "测试向量："
foreach ($v in $vectors) {
    Write-Host ("  [{0}] 期望 CRC = 0x{1:X4}" -f $v.desc, $v.expect)
}
Write-Host ""

foreach ($algo in $algos) {
    $table = Build-Crc16Table -poly $algo.poly -reflected $algo.ref
    $allPass = $true
    $results = @()

    foreach ($v in $vectors) {
        $bytes = $v.hex -split ' ' | ForEach-Object { [byte]('0x' + $_) }
        $crc = Calc-Crc16 -table $table -reflected $algo.ref -init $algo.init -xorOut $algo.xor -data $bytes
        $pass = ($crc -eq $v.expect)
        if (-not $pass) { $allPass = $false }
        $results += ("    [{0}] calc=0x{1:X4} expect=0x{2:X4} {3}" -f $v.desc, $crc, $v.expect, $(if ($pass) {"✓"} else {"✗"}))
    }

    $status = if ($allPass) { "[全部通过 ✓✓✓]" } else { "[部分失败]" }
    Write-Host ("{0} {1}" -f $status, $algo.name)
    foreach ($r in $results) { Write-Host $r }
    Write-Host ""
}

# 额外：输出 SET_COM "0F 01" 在每种算法下的结果，供对照
Write-Host "========== SET_COM 0F 01 各算法计算结果 =========="
$setComBytes = @([byte]0x0F, [byte]0x01)
foreach ($algo in $algos) {
    $table = Build-Crc16Table -poly $algo.poly -reflected $algo.ref
    $crc = Calc-Crc16 -table $table -reflected $algo.ref -init $algo.init -xorOut $algo.xor -data $setComBytes
    Write-Host ("  0x{0:X4}  ← {1}" -f $crc, $algo.name)
}
