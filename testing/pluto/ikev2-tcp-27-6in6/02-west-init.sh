/testing/guestbin/swan-prep --46

# confirm that the network is alive
../../guestbin/ping-once.sh --up -I 2001:db8:0:1::254 2001:db8:0:2::254

# ensure that clear text does not get through
ip6tables -A INPUT -i eth1 -s 2001:db8:0:2::254 -p ipv6-icmp --icmpv6-type echo-reply  -j DROP
ip6tables -I INPUT -m policy --dir in --pol ipsec -j ACCEPT

# confirm clear text does not get through
../../guestbin/ping-once.sh --down -I 2001:db8:0:1::254 2001:db8:0:2::254

ipsec start
../../guestbin/wait-until-pluto-started
ipsec add 6in6
ipsec whack --impair suppress-retransmits
