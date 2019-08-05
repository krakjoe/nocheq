--TEST--
Basic strict types compatibility
--FILE--
<?php
declare(strict_types=1);

function expectsFloatsGetsInts(float $int, float ... $ints) {
    return [$int, $ints];
}

function expectedToReturnFloatReturnsInt() : float {
    return 1;
}

var_dump(
    expectsFloatsGetsInts(1, 2, 3, 4), 
    expectedToReturnFloatReturnsInt());
?>
--EXPECT--
array(2) {
  [0]=>
  float(1)
  [1]=>
  array(3) {
    [0]=>
    float(2)
    [1]=>
    float(3)
    [2]=>
    float(4)
  }
}
float(1)

