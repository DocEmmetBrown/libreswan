ipsec whack --impair revival
ipsec up west

ipsec whack --impair trigger-revival:1

../../guestbin/ping-once.sh --up -I 192.0.1.254 192.0.2.254
ipsec whack --trafficstatus

# should show tcp being used
../../guestbin/ipsec-kernel-state.sh
../../guestbin/ipsec-kernel-policy.sh
