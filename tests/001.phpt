--TEST--
Basic nocheq is working on parameters
--FILE--
<?php
declare(strict_types=1);

function nocheq(int $arg) {
    var_dump($arg);
}

nocheq("string");
?>
--EXPECT--
string(6) "string"
