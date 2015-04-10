#!/bin/sh

lupdate src/ qml/ -ts translations/*
#sed -i translations/*.ts -e 's/><\/translation>/\/>/'
