#!/bin/sh
exec sed -n '/^VERSION=/s:.*=::p' "$1"/Makefile
