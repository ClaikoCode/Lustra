# Task Document

This is a document that contains a list of tasks that I can get to when I have the time for them. These tasks can either be features I want to implement, code refactors that need to be done, or bugs that I want to fix. It is a way for me to remember things I want to get done but maybe cant do at the moment I thought of them.

## Tasks

### Bake sampler indices into a single data field on the GPU side

Currently, there are 5 uints stored, one for each type of sampler index that is used for the different material maps. However, there will most likely never be a time where I will have over 255 unique samplers, which means that they all could fit into at least two 32 bit uints instead.
