--TEST--
Basic nocheq is working on return
--FILE--
<?php
declare(strict_types=1);

function nocheq(string $arg) : int {
    return $arg;
}

var_dump(nocheq("string"));
?>
--EXPECT--
string(6) "string"
