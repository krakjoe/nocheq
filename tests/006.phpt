--TEST--
Basic nocheq.namespace
--INI--
nocheq.namespace=foo
--FILE--
<?php
namespace Foo {
    function bar(int $thing) {
        var_dump($thing);
    }
}

namespace Bar {
    function qux(int $thing) {
        var_dump($thing);
    }
}

namespace {
    Foo\bar('string');

    try {
        Bar\qux('string');
    } catch (TypeError $ex) {
        echo "OK\n";
    }
}
?>
--EXPECT--
string(6) "string"
OK
