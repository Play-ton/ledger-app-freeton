#!/bin/bash
pyinstaller --clean -y -F freetoncli.py --collect-all tonclient -i crystal.ico
