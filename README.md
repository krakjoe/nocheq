# nocheq

This is an experiment, in response to:

    https://en.reddit.com/r/PHP/comments/cl2p1z/something_to_consider_what_about_disabling/

I don't know what kind of performance difference this will have for your code, but if it's remarkable then start opening issues and suggesting improvements to this idea.

## install nocheq

To build:

    phpize
    ./configure
    make install

To install:

    extension=nocheq.so

### How it Works

Well tested code may have type checking disabled in production, if it is developed and tested using strict type checking: nocheq rewrites the instruction handlers for RECV and RETURN, so that any code compiled with `declare(strict_types=1);` may avoid type checking in production.

Simple.

#### Will you work on this ?

I haven't implemented support for avoiding type checks on typed properties right now.

If I'm presented with really good justification for the continued development of this extension then I'll work on it, otherwise it was a fun Sunday thing that kept me occupied for half an hour.

Peace out phomies ...
