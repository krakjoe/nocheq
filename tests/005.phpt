--TEST--
Basic nocheq is working on parameters with default value
--FILE--
<?php
function nocheq(int $arg = 5) {
    var_dump($arg);
}

nocheq("string");
?>
--EXPECT--
string(6) "string"
