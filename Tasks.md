# Task Document

This is a document that contains a list of tasks that I can get to when I have the time for them. These tasks can either be features I want to implement, code refactors that need to be done, or bugs that I want to fix. It is a way for me to remember things I want to get done but maybe cant do at the moment I thought of them.

## Tasks

### Bake sampler indices into a single data field on the GPU side

Currently, there are 5 uints stored, one for each type of sampler index that is used for the different material maps. However, there will most likely never be a time where I will have over 255 unique samplers, which means that they all could fit into at least two 32 bit uints instead.

### Add interface in DirectoryWatcher to handle new files and deleted files.

Directory watcher blindly adds and removes new files without reporting to the user what has changed. Newly added files get added to updated files but some systems might not desire that functionality and instead want to have the information to act accordingly.

### Try out more spaced out log prints

Something like:
std::print(outputFile, "[{}] ({} -> {}:{})\n{}\n\n", outputLevelString, funcName, fileName, line, formattedMessage);

This would allow for easier formatting in some cases but might be harder in others. For example, all iterative outputs should instead be formatted first because it is hard to read when they are not consecutive anymore with the additional spacing.

