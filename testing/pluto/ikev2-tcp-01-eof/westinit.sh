/testing/guestbin/swan-prep
# dmesg -n 6
# nohup tcpdump -i eth1 -s 65535 -X -vv -nn tcp > OUTPUT/west.tcpdump & sleep 1 # wait for nohup msg
# nohup dumpcap -i eth1 -w /tmp/west.pcap > OUTPUT/west.dumpcap & sleep 1 # wait for nohup msg
# confirm that the network is alive
# ../../guestbin/wait-until-alive -I 192.0.1.254 192.0.2.254
# make sure that clear text does not get through
# iptables -F
# iptables -X
# does this block the ping response?
# iptables -A OUTPUT -o eth1 -p tcp --dport 4500 -j ACCEPT
# iptables -A OUTPUT -o eth1 -s 192.0.2.0/24 -j DROP
# confirm with a ping
# ../../guestbin/ping-once.sh --down -I 192.0.1.254 192.0.2.254
ipsec start
../../guestbin/wait-until-pluto-started
ipsec add westnet-eastnet-ikev2
ipsec auto --status | grep westnet-eastnet-ikev2
echo "initdone"
