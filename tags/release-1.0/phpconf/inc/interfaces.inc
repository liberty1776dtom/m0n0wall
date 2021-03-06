<?php
/*
	interfaces.inc
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2003-2004 Manuel Kasper <mk@neon1.net>.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.
	
	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

/* include all configuration functions */
require_once("functions.inc");

function interfaces_loopback_configure() {
	global $config, $g;

	mwexec("/sbin/ifconfig lo0 127.0.0.1");
	
	return 0;
}

function interfaces_lan_configure() {
	global $config, $g;
	
	if ($g['booting'])
		echo "Configuring LAN interface... ";
	
	$lancfg = $config['interfaces']['lan'];
		
	/* wireless configuration? */
	if (is_array($lancfg['wireless']))
		interfaces_wireless_configure($lancfg['if'], $lancfg['wireless']);
	
	/* MAC spoofing? */
	if ($lancfg['spoofmac'])
		mwexec("/sbin/ifconfig " . escapeshellarg($lancfg['if']) . 
			" link " . escapeshellarg($lancfg['spoofmac']));
	
	mwexec("/sbin/ifconfig " . escapeshellarg($lancfg['if']) . " " . 
		escapeshellarg($lancfg['ipaddr'] . "/" . $lancfg['subnet']));
	
	if (!$g['booting']) {
		/* make new hosts file */
		system_hosts_generate();
		
		/* reconfigure static routes (kernel may have deleted them) */
		system_routing_configure();
		
		/* reload ipfilter (address may have changed) */
		filter_configure();
		
		/* reload shaper (subnet may have changed) */
		shaper_configure();
		
		/* reload IPsec tunnels */
		vpn_ipsec_configure();
		
		/* reload dhcpd (gateway may have changed) */
		services_dhcpd_configure();
		
		/* reload dnsmasq */
		services_dnsmasq_configure();
		
		/* reload webgui */
		system_webgui_start();
	}
	
	if ($g['booting'])
		echo "done\n";
	
	return 0;
}

function interfaces_optional_configure() {
	global $config, $g;
	global $bridgeconfig;
	
	/* Reset bridge configuration.	Interfaces will add to it. */
	$bridgeconfig = "";
	
	for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
		interfaces_optional_configure_if($i);
	}
	
	if ($bridgeconfig) {
		/* Set the system bridge configuration and enable bridging. */
		mwexec("/sbin/sysctl net.link.ether.bridge_cfg=" . $bridgeconfig);
		
		if (isset($config['bridge']['filteringbridge']))
			mwexec("/sbin/sysctl net.link.ether.bridge_ipf=1");
		
		mwexec("/sbin/sysctl net.link.ether.bridge=1");
	} else {
		mwexec("/sbin/sysctl net.link.ether.bridge_ipf=0");
		mwexec("/sbin/sysctl net.link.ether.bridge=0");
	}
	
	if (!$g['booting']) {
		/* reconfigure static routes (kernel may have deleted them) */
		system_routing_configure();
		
		/* reload ipfilter (address may have changed) */
		filter_configure();
		
		/* reload shaper (address may have changed) */
		shaper_configure();
		
		/* reload IPsec tunnels */
		vpn_ipsec_configure();
		
		/* reload dhcpd (interface enabled/disabled/bridged status may have changed) */
		services_dhcpd_configure();
		
		/* restart dnsmasq */
		services_dnsmasq_configure();
	}
	
	return 0;
}

function interfaces_optional_configure_if($opti) {
	global $config, $g;
	global $bridgeconfig;
	
	$optcfg = $config['interfaces']['opt' . $opti];
	
	if ($g['booting']) {
		$optdescr = "";
		if ($optcfg['descr'])
			$optdescr = " ({$optcfg['descr']})";
		echo "Configuring OPT{$opti}{$optdescr} interface... ";
	}
	
	if (isset($optcfg['enable'])) {
		/* wireless configuration? */
		if (is_array($optcfg['wireless']))
			interfaces_wireless_configure($optcfg['if'], $optcfg['wireless']);
		
		/* MAC spoofing? */
		if ($optcfg['spoofmac'])
			mwexec("/sbin/ifconfig " . escapeshellarg($optcfg['if']) . 
				" link " . escapeshellarg($optcfg['spoofmac']));
		
		/* bridged? */
		if ($optcfg['bridge']) {
			mwexec("/sbin/ifconfig " . escapeshellarg($optcfg['if']) . 
				" delete up");
			
			if ($bridgeconfig != "")
				$bridgeconfig .= ",";
			
			$bridgeconfig .= $optcfg['if'] . ":" . $opti . "," .
				$config['interfaces'][$optcfg['bridge']]['if'] .
				":" . $opti;
		} else {
			mwexec("/sbin/ifconfig " . escapeshellarg($optcfg['if']) . " " . 
				escapeshellarg($optcfg['ipaddr'] . "/" . $optcfg['subnet']));
		}
	} else {
		mwexec("/sbin/ifconfig " . escapeshellarg($optcfg['if']) . 
			" delete down");
	}
	
	if ($g['booting'])
		echo "done\n";
	
	return 0;
}

function interfaces_wireless_configure($if, $wlcfg) {
	global $config, $g;
	
	/* wireless configuration */
	$ifcargs = escapeshellarg($if) . 
		" ssid " . escapeshellarg($wlcfg['ssid']) . " channel " . 
		escapeshellarg($wlcfg['channel']) . " ";
	
	if ($wlcfg['stationname'])
		$ifcargs .= "stationname " . escapeshellarg($wlcfg['stationname']) . " ";
	
	if (isset($wlcfg['wep']['enable']) && is_array($wlcfg['wep']['key'])) {
		$ifcargs .= "wepmode on ";
		
		$i = 1;
		foreach ($wlcfg['wep']['key'] as $wepkey) {
			$ifcargs .= "wepkey " . escapeshellarg("{$i}:{$wepkey['value']}") . " ";
			if (isset($wepkey['txkey'])) {
				$ifcargs .= "weptxkey {$i} ";
			}
			$i++;
		}
	} else {
		$ifcargs .= "wepmode off ";
	}
	
	switch ($wlcfg['mode']) {
		case 'hostap':
			if (strstr($if, "wi"))
				$ifcargs .= "-mediaopt ibss mediaopt hostap ";
			break;
		case 'ibss':
		case 'IBSS':
			if (strstr($if, "wi"))
				$ifcargs .= "-mediaopt hostap mediaopt ibss ";
			else if (strstr($if, "an"))
				$ifcargs .= "mediaopt adhoc ";
			break;
		case 'bss':
		case 'BSS':
			if (strstr($if, "wi"))
				$ifcargs .= "-mediaopt hostap -mediaopt ibss ";
			else if (strstr($if, "an"))
				$ifcargs .= "-mediaopt adhoc ";
			break;
	}
	
	$ifcargs .= "up";
	
	mwexec("/sbin/ifconfig " . $ifcargs);
	
	return 0;
}

function interfaces_wan_configure() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	
	if ($g['booting'])
		echo "Configuring WAN interface... ";
	else {
		/* kill dhclient */
		killbypid("{$g['varrun_path']}/dhclient.pid");
		
		/* kill PPPoE client (mpd) */
		killbypid("{$g['varrun_path']}/mpd.pid");
		
		/* wait for processes to die */
		sleep(2);
		
		/* remove dhclient.conf, if it exists */
		if (file_exists("{$g['varetc_path']}/dhclient.conf")) {
			unlink("{$g['varetc_path']}/dhclient.conf");
		}
		/* remove mpd.conf, if it exists */
		if (file_exists("{$g['varetc_path']}/mpd.conf")) {
			unlink("{$g['varetc_path']}/mpd.conf");
		}
		/* remove mpd.links, if it exists */
		if (file_exists("{$g['varetc_path']}/mpd.links")) {
			unlink("{$g['varetc_path']}/mpd.links");
		}
	}
	
	/* remove all addresses first */
	while (mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " -alias") == 0);
	mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " down");
		
	/* wireless configuration? */
	if (is_array($wancfg['wireless']))
		interfaces_wireless_configure($wancfg['if'], $wancfg['wireless']);
	
	if ($wancfg['spoofmac'])
		mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . 
			" link " . escapeshellarg($wancfg['spoofmac']));
	
	switch ($wancfg['ipaddr']) {
	
		case 'dhcp':
			interfaces_wan_dhcp_configure();
			break;
			
		case 'pppoe':
			interfaces_wan_pppoe_configure();
			break;
			
		case 'pptp':
			interfaces_wan_pptp_configure();
			break;
			
		default:
			mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " " . 
				escapeshellarg($wancfg['ipaddr'] . "/" . $wancfg['subnet']));
			
			/* install default route */
			mwexec("/sbin/route delete default");
			mwexec("/sbin/route add default " . escapeshellarg($wancfg['gateway']));
			
			/* resync ipfilter (done automatically for DHCP/PPPoE/PPTP) */
			filter_resync();
	}
	
	if (!$g['booting']) {
		/* reconfigure static routes (kernel may have deleted them) */
		system_routing_configure();
		
		/* reload ipfilter */
		filter_configure();
		
		/* reload shaper */
		shaper_configure();
		
		/* reload ipsec tunnels */
		vpn_ipsec_configure();
		
		/* restart ez-ipupdate */
		services_dyndns_configure();
		
		/* restart dnsmasq */
		services_dnsmasq_configure();
	}
	
	if ($g['booting'])
		echo "done\n";
	
	return 0;
}

function interfaces_wan_dhcp_configure() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];

	/* generate dhclient.conf */
	$fd = fopen("{$g['varetc_path']}/dhclient.conf", "w");
	if (!$fd) {
		printf("Error: cannot open dhclient.conf in interfaces_wan_dhcp_configure().\n");
		return 1;
	}
 
 	$dhclientconf = "";
 
 	if ($wancfg['dhcphostname']) {
		$dhclientconf .= <<<EOD
send dhcp-client-identifier "{$wancfg['dhcphostname']}";
interface "{$wancfg['if']}" {
	send host-name "{$wancfg['dhcphostname']}";
}

EOD;
	}

	fwrite($fd, $dhclientconf);
	fclose($fd);
	
	/* fire up dhclient - don't wait for the lease (-nw) */
	mwexec("/sbin/dhclient -nw -cf {$g['varetc_path']}/dhclient.conf " .
		escapeshellarg($wancfg['if']));
	
	return 0;
}

function interfaces_wan_pppoe_configure() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	$pppoecfg = $config['pppoe'];
	
	/* generate mpd.conf */
	$fd = fopen("{$g['varetc_path']}/mpd.conf", "w");
	if (!$fd) {
		printf("Error: cannot open mpd.conf in interfaces_wan_pppoe_configure().\n");
		return 1;
	}
		
	$mpdconf = <<<EOD
pppoe:
	new -i ng0 pppoe pppoe
	set iface route default
	set iface disable on-demand
	set iface idle 0
	set iface up-script /usr/local/sbin/ppp-linkup
	set bundle disable multilink
	set bundle authname "{$pppoecfg['username']}"
	set bundle password "{$pppoecfg['password']}"
	set link keep-alive 10 60
	set link max-redial 0
	set link no acfcomp protocomp
	set link disable pap chap
	set link accept chap
	set link mtu 1492
	set ipcp yes vjcomp
	set ipcp ranges 0.0.0.0/0 0.0.0.0/0
	open iface
	
EOD;

	fwrite($fd, $mpdconf);
	fclose($fd);
	
	/* generate mpd.links */
	$fd = fopen("{$g['varetc_path']}/mpd.links", "w");
	if (!$fd) {
		printf("Error: cannot open mpd.links in interfaces_wan_pppoe_configure().\n");
		return 1;
	}
		
	$mpdconf = <<<EOD
pppoe:
	set link type pppoe
	set pppoe iface {$wancfg['if']}
	set pppoe service "{$pppoecfg['provider']}"
	
EOD;

	fwrite($fd, $mpdconf);
	fclose($fd);
	
	/* fire up mpd */
	mwexec("/usr/local/sbin/mpd -b -d {$g['varetc_path']} -p {$g['varrun_path']}/mpd.pid pppoe");
	
	return 0;
}

function interfaces_wan_pptp_configure() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	$pptpcfg = $config['pptp'];
	
	/* generate mpd.conf */
	$fd = fopen("{$g['varetc_path']}/mpd.conf", "w");
	if (!$fd) {
		printf("Error: cannot open mpd.conf in interfaces_wan_pptp_configure().\n");
		return 1;
	}
		
	$mpdconf = <<<EOD
pptp:
	new -i ng0 pptp pptp
	set iface route default
	set iface disable on-demand
	set iface idle 0
	set iface up-script /usr/local/sbin/ppp-linkup
	set bundle disable multilink
	set bundle authname "{$pptpcfg['username']}"
	set bundle password "{$pptpcfg['password']}"
	set link keep-alive 10 60
	set link max-redial 0
	set link no acfcomp protocomp
	set link disable pap chap
	set link accept chap
	set ipcp no vjcomp
	set ipcp ranges 0.0.0.0/0 0.0.0.0/0
	open
	
EOD;

	fwrite($fd, $mpdconf);
	fclose($fd);
	
	/* generate mpd.links */
	$fd = fopen("{$g['varetc_path']}/mpd.links", "w");
	if (!$fd) {
		printf("Error: cannot open mpd.links in interfaces_wan_pptp_configure().\n");
		return 1;
	}
		
	$mpdconf = <<<EOD
pptp:
	set link type pptp
	set pptp mode active
	set pptp enable originate outcall
	set pptp self {$pptpcfg['local']}
	set pptp peer {$pptpcfg['remote']}
	
EOD;

	fwrite($fd, $mpdconf);
	fclose($fd);
	
	/* configure interface */
	mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " " . 
		escapeshellarg($pptpcfg['local'] . "/" . $pptpcfg['subnet']));
	
	/* fire up mpd */
	mwexec("/usr/local/sbin/mpd -b -d {$g['varetc_path']} -p {$g['varrun_path']}/mpd.pid pptp");
	
	return 0;
}

function get_real_wan_interface() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	
	$wanif = $wancfg['if'];
	if (($wancfg['ipaddr'] == "pppoe") || ($wancfg['ipaddr'] == "pptp")) {
		$wanif = $g['pppoe_interface'];
	}
	
	return $wanif;
}

function get_current_wan_address() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	
	if (in_array($wancfg['ipaddr'], array('pppoe','dhcp','pptp'))) {
		/* dynamic WAN IP address, find out which one */
		$wanif = get_real_wan_interface();
		
		/* get interface info with netstat */
		exec("/usr/bin/netstat -nWI " . escapeshellarg($wanif) . " -f inet", $ifinfo);
		
		if (isset($ifinfo[1])) {
			$aif = preg_split("/\s+/", $ifinfo[1]);
			$curwanip = chop($aif[3]);
			
			if ($curwanip && is_ipaddr($curwanip) && ($curwanip != "0.0.0.0"))
				return $curwanip;
		}
		
		return null;
	} else {
		/* static WAN IP address */
		return $wancfg['ipaddr'];
	}
}

?>
