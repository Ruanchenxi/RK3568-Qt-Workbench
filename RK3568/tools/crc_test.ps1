function crc16([byte[]]$data,[uint16]$poly,[bool]$ref,[uint16]$init,[uint16]$xorOut){
    $t=New-Object uint16[] 256
    for($i=0;$i-lt256;$i++){
        $c=[uint16]$i
        if($ref){ for($j=0;$j-lt8;$j++){ if($c-band 1){$c=[uint16](($c-shr 1)-bxor$poly)}else{$c=[uint16]($c-shr 1)} } }
        else    { $c=[uint16]($i-shl 8); for($j=0;$j-lt8;$j++){ if($c-band 32768){$c=[uint16](($c-shl 1)-bxor$poly)}else{$c=[uint16]($c-shl 1)} } }
        $t[$i]=$c
    }
    $crc=$init
    foreach($b in $data){
        if($ref){ $crc=[uint16](($crc-shr 8)-bxor$t[($crc-bxor$b)-band 255]) }
        else    { $crc=[uint16](($crc-shl 8)-bxor$t[(($crc-shr 8)-bxor$b)-band 255]) }
    }
    return [uint16]($crc-bxor$xorOut)
}

$vecs=@(
    @{d=[byte[]](0x11,0x00,0x00,0x00,0x00);e=[uint16]0x44A0;n="KEY_REMOVED"}
    @{d=[byte[]](0x11,0x01,0x00,0x00,0x00);e=[uint16]0xD162;n="KEY_PLACED "}
    @{d=[byte[]](0x00,0x0F,0x03);          e=[uint16]0x3C53;n="NAK/SETCOM "}
    @{d=[byte[]](0x5A,0x0F);               e=[uint16]0xA720;n="ACK/SETCOM "}
    @{d=[byte[]](0x0F,0x01);               e=[uint16]0x1C11;n="TX_SETCOM  "}
)
$algos=@(
    @{n="XMODEM       poly=1021 noRef init=0000";  p=[uint16]0x1021;r=$false;i=[uint16]0x0000;x=[uint16]0x0000}
    @{n="CCITT-FALSE  poly=1021 noRef init=FFFF";  p=[uint16]0x1021;r=$false;i=[uint16]0xFFFF;x=[uint16]0x0000}
    @{n="KERMIT       poly=1021 Ref   init=0000";  p=[uint16]0x8408;r=$true; i=[uint16]0x0000;x=[uint16]0x0000}
    @{n="X.25         poly=1021 Ref   init=FFFF xorFFFF"; p=[uint16]0x8408;r=$true;i=[uint16]0xFFFF;x=[uint16]0xFFFF}
    @{n="MCRF4XX      poly=1021 Ref   init=FFFF"; p=[uint16]0x8408;r=$true; i=[uint16]0xFFFF;x=[uint16]0x0000}
    @{n="IBM/ARC      poly=8005 Ref   init=0000";  p=[uint16]0xA001;r=$true; i=[uint16]0x0000;x=[uint16]0x0000}
    @{n="MODBUS       poly=8005 Ref   init=FFFF";  p=[uint16]0xA001;r=$true; i=[uint16]0xFFFF;x=[uint16]0x0000}
    @{n="BUYPASS      poly=8005 noRef init=0000";  p=[uint16]0x8005;r=$false;i=[uint16]0x0000;x=[uint16]0x0000}
)
foreach($a in $algos){
    $pass=$true; $row=""
    foreach($v in $vecs){
        $c=crc16 $v.d $a.p $a.r $a.i $a.x
        $ok=if($c-eq$v.e){"OK"}else{"NO"}
        if($c-ne$v.e){$pass=$false}
        $row+="  $($v.n):calc=$('{0:X4}'-f$c) exp=$('{0:X4}'-f$v.e) $ok"
    }
    $mark=if($pass){"[ALL PASS]"}else{"[        ]"}
    Write-Host "$mark $($a.n)"
    Write-Host "          $row"
}
