--TEST--
Basic nocheq is working on variadics
--FILE--
<?php
function nocheq(int ... $args) {
    var_dump(...$args);
}

nocheq("s", "t", "r", "i", "n", "g");
?>
--EXPECT--
string(1) "s"
string(1) "t"
string(1) "r"
string(1) "i"
string(1) "n"
string(1) "g"

