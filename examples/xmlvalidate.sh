#!/bin/sh

SCHEME="EtherCATInfo.xsd"

COMMAND="xmllint --noout --schema ${SCHEME}"


for f in Somanet_CiA402-multi.xml Somanet_CiA402-single.xml Somanet_CtrlProto.xml 
do
	echo "Checking $f"
	eval ${COMMAND} ${f}
done
