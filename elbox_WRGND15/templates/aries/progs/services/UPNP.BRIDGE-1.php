<?
include "/etc/services/HTTP/httpsvcs.php";
fwrite("w",$START,"#!/bin/sh\n");
fwrite("w", $STOP,"#!/bin/sh\n");
upnpsetup("BRIDGE-1");
fwrite("a",$START,"exit 0\n");
fwrite("a", $STOP,"exit 0\n");
?>
