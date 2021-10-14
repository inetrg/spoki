#!/bin/bash

iptables -I OUTPUT -p tcp --tcp-flags ALL RST,ACK -j DROP -m comment --comment "spoki-rst-rule"
