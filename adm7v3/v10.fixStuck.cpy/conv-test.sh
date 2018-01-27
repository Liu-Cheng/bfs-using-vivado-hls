#!/bin/bash
#ARGS="--test 3 -m 20 -imn 100 -imc 1 -imh 28 -imw 28 -kh 5 -kw 5 -ph 0 -pw 0 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # lenet conv1, cout=20
#mv t.log im2col-test.log
ARGS="--test 3 -m 50 -imn 100 -imc 20 -imh 12 -imw 12 -kh 5 -kw 5 -ph 0 -pw 0 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # lenet conv2, cout=50
#cat t.log >> im2col-test.log
#ARGS="--test 3 -m 32 -imn 100 -imc 3 -imh 32 -imw 32 -kh 5 -kw 5 -ph 2 -pw 2 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # cifar10 conv1, cout=32
#cat t.log >> im2col-test.log
#ARGS="--test 3 -m 32 -imn 100 -imc 32 -imh 16 -imw 16 -kh 5 -kw 5 -ph 2 -pw 2 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # cifar10 conv2, cout=32
#cat t.log >> im2col-test.log
#ARGS="--test 3 -m 64 -imn 100 -imc 32 -imh 8 -imw 8 -kh 5 -kw 5 -ph 2 -pw 2 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # cifar10 conv3, cout=64
#cat t.log >> im2col-test.log
#
#ARGS="--test 3 -m 96 -imn 50 -imc 3 -imh 227 -imw 227 -kh 11 -kw 11 -ph 0 -pw 0 -sh 4 -sw 4 -dh 1 -dw 1" ./run.sh check # caffenet conv1, cout=96
#cat t.log >> im2col-test.log
#
#ARGS="--test 3 -m 256 -imn 50 -imc 96 -imh 27 -imw 27 -kh 5 -kw 5 -ph 2 -pw 2 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # caffenet conv2, cout=256
##mv t.log im2col-test.log
#cat t.log >> im2col-test.log
#ARGS="--test 3 -m 384 -imn 50 -imc 256 -imh 13 -imw 13 -kh 3 -kw 3 -ph 1 -pw 1 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # caffenet conv3, cout=384
#cat t.log >> im2col-test.log
#ARGS="--test 3 -m 384 -imn 50 -imc 384 -imh 13 -imw 13 -kh 3 -kw 3 -ph 1 -pw 1 -sh 1 -sw 1 -dh 1 -dw 1" ./run.sh check # caffenet conv4(384), conv5(256)
#cat t.log >> im2col-test.log

