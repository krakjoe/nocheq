--TEST--
Basic nocheq is working on parameters
--FILE--
<?php
function nocheq(int $arg) {
    var_dump($arg);
}

nocheq("string");
?>
--EXPECT--
string(6) "string"
