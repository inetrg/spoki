#!/bin/bash

iptables-save | grep -v "spoki-rst-rule" | iptables-restore
