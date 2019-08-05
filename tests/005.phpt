--TEST--
Basic nocheq is working on parameters with default value
--FILE--
<?php
declare(strict_types=1);

function nocheq(int $arg = 5) {
    var_dump($arg);
}

nocheq("string");
?>
--EXPECT--
string(6) "string"
