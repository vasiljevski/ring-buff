# ring-buff #

## Introduction ##
Idea of this project is to encapsulate one functionality which was re-implemented thousand times - **ring (circular) buffer**.

## Features ##
#### Portable ####
Code is written in pure  C (C99 compatible). All OS facilities are aggregated in simple OS abstraction layer. As a show case POSIX implementation is available for OS abstraction layer, but porting can be done easily to most general or RT operating systems.
#### Embeddable ####
Implemented ring buffer contains only two modules (one for OS abstraction layer and one for actual implementation). As such it can be imported to bigger projects easily.
#### Easy to use ####
Clear cut API makes module usage as easy as possible. ring-buff is written in object oriented way (although written in C), with distinctive data model, and functions which are handling the data.
#### Thread safe ####
In general, ring-buffer is designed for multithreaded environment. All functions are protected from concurrent access.
#### Solving producer/consumer problem ####
Reader/writer model of ring buffer is supported. In this case data is copied during reading operation.
#### Accumulation (zero copy) ####
Streaming friendly mode is supported with accumulation callback. In this case, ring-buffer will notify consumer that enough **continuous** data is available for consumption.


## How to help? ##
Using the code and reporting bugs and problems is sufficient. Ideas on how to extend the functionality and patches are very welcome. Thanks!