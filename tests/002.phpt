--TEST--
Basic nocheq doesn't break parameters with default value
--FILE--
<?php
function nocheq(string $arg = 'string') {
    var_dump($arg);
}

nocheq();
?>
--EXPECT--
string(6) "string"
