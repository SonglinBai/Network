//
// Created by 白松林 on 2021/5/8.
//
#include <iostream>
#include <Carbon/Carbon.h>

Boolean isPressed(unsigned short inKeyCode) {
    unsigned char keyMap[16];
    GetKeys((BigEndianUInt32 * ) & keyMap);
    return (0 != ((keyMap[inKeyCode >> 3] >> (inKeyCode & 7)) & 1));
}

int main() {

    bool bQuit = false;
    while (!bQuit) {
        if (isPressed(kVK_ANSI_Q)) {
            bQuit = true;
            std::cout << "key pressed" << std::endl;
        }
    }

}