swanctl --initiate --child westnet-eastnet
ping -n -q -w 4 -c 4 -I 192.0.1.254 192.0.2.254
ip xfrm policy
../../guestbin/ipsec-kernel-state.sh
ip link set up dev ipsec0
ip route add 192.0.2.0/24 dev ipsec0
ping -n -q -w 4 -c 4 -I 192.0.1.254 192.0.2.254
