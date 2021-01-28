/testing/guestbin/swan-prep --userland strongswan
# force the number of threads
sed -i -e 's/threads[ ]*=[ 0-9]*$/threads = 6/' /etc/strongswan/strongswan.conf
# confirm that the network is alive
../../pluto/bin/wait-until-alive -I 192.0.1.254 192.0.2.254
# ensure that clear text does not get through
iptables -A INPUT -i eth1 -s 192.0.2.0/24 -j DROP
iptables -I INPUT -m policy --dir in --pol ipsec -j ACCEPT
# confirm clear text does not get through
../../pluto/bin/ping-once.sh --down -I 192.0.1.254 192.0.2.254
cp /testing/x509/certs/east.crt /etc/strongswan/ipsec.d/certs/
cp /testing/x509/cacerts/mainca.crt /etc/strongswan/ipsec.d/cacerts/
../../pluto/bin/strongswan-start.sh
echo "initdone"
