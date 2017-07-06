libevl
===========

`libevl` Yet Another(tm) library for abstracting event handling,
like `libevent` or `libev`. It aims to be familiar to users of
`libevent`, and simple and robust to use.

This was written as a result of spending time reimplimenting the
`libevent` API, which led to the realisation that users of the API
generally do not correctly handle errors, ie, noone checks for the
errors that `event_add()` and `event_del()` return. Additionally,
some aspects of the API are hard to reason about, eg, it is not
obvious what will happen with an event set with `EV_PERSIST` and a
timeout specified in `event_add()`. Those two main concerns led to
the main differences between `libevl` and `libevent`.

Firstly, file descriptor monitoring and timeout handling are separated
into different types and interfaces to avoid confusion about their
interactions as found in `libevent`'s `event_add()` interface.
File descriptor monitoring is handled with `struct evl_io`, and
timeouts are handled with `struct evl_tmo`.

Secondly, the API is arranged so adding and deleteing events cannot
fail. Instead, all the steps that may fail when setting up event
handling in a backend (e.g., poll or kqueue) are pushed to the
allocation of the event. This also allows for hiding of the contents
of the various structures.

Todo
---

- Implement a `poll()` backend.
- Implement an event port backend for Solaris and Illumos.
- Implement abstraction of signal handling
- Implement abstraction of `wait()` handling.

Why?
----

I'm an idiot.
