#!/usr/bin/python3

import random
import sys

def random_char():
    return chr(random.randint(ord('a'), ord('z')))

def random_str(len):
    for i in range(len):
        print(random_char(), end='')

random_str(int(sys.argv[1]))
