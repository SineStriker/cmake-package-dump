#ifndef RESOURCES_H
#define RESOURCES_H

struct BinaryData {
    const unsigned char *data;
    const unsigned int size;
};

extern struct BinaryData TestTargets_cmake_data;

extern struct BinaryData CMakeLists_txt_data;

#endif // RESOURCES_H