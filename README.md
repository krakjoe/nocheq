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

## configure nocheq

Restrict nocheq to a namespace:

    nocheq.namespace=\\Vendor\\Namespace

It is probably necessary to restrict to your vendor namespace ...

### How it Works

nocheq rewrites the instruction handlers for RECV and RETURN opcodes to avoid type checking.

Simple.

#### Will you work on this ?

I haven't implemented support for avoiding type checks on typed properties right now.

If I'm presented with really good justification for the continued development of this extension then I'll work on it, otherwise it was a fun Sunday thing that kept me occupied for half an hour.

Peace out phomies ...
