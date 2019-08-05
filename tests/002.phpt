--TEST--
Basic nocheq doesn't break parameters with default value
--FILE--
<?php
declare(strict_types=1);

function nocheq(string $arg = 'string') {
    var_dump($arg);
}

nocheq();
?>
--EXPECT--
string(6) "string"
