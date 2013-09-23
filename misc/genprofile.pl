#!/usr/bin/perl 
#
# generate the xml section <profile>...</profile> from the source
# codes object dictionary.
#
# TODO:
# - read object description section
# - write profile section
# - write object description to datatype section <- check how to
# - respect subindex values in array and generic datatypes
#
# 2013-09-23 Frank Jeschke <fjeschke@synapticon.com>

use strict;
use warnings;

sub readod {
	my ( $filename ) = @_;
	my $state = 0; # 0- nothing, 1- read description, 2 - read objects
	my @objects;

	printf("Read OD from file $filename\n");
	open (my $FH, $filename) or die "Can't open file";

	while (<$FH>) {
		if (m/^struct _sdoinfo_entry_description SDO_Info_Entries\[\] = {/) {
			$state = 1;
			next;
		}

		if ($state == 1) {
			#printf "have to read the entries\n";

			if (m/\{ 0, 0, 0, 0, 0, 0, 0, "\\0" \}/) {
				$state = 0;
				#printf "fin entries\n";
				next;
			}

			#if (m/\s*?[\/\*#].*/) { # ignore comments
			#next;
			#}
			if (!m/\{.*\},/) {
				#printf "no valid entry match, next iteration\n";
				next;
			}

			my $tmpobj = {};
			chop $_;
			#printf "DBG: '$_'\n";
			m/\{\s*(\S+)\s*,\s*(\S+)\s*,\s*(\S+)\s*,\s*(\S+)\s*,\s*(\S+)\s*,\s*(.+?)\s*,\s*(\S+)\s*,\s*"(.+)"/;
			#printf "Should load: '$1' '$2' '$3' '$4' '$5' '$6' '$7' '$8' to hash\n";
			$tmpobj->{"index"} = $1;
			$tmpobj->{"subindex"} = $2;
			$tmpobj->{"valueinfo"} = $3;
			$tmpobj->{"datatype"} = $4;
			$tmpobj->{"bitsize"} = $5;
			$tmpobj->{"access"} = $6;
			$tmpobj->{"defaultvalue"} = $7;
			$tmpobj->{"description"} = $8;
			push @objects, $tmpobj;
		}
	}

	close $FH;

	return @objects;
}

sub printthis {
	my (@objects ) = @_;

	foreach my $obj (@objects) {
		printf "object " .$obj->{"index"}. "\n";
	}
}

# Info of hash fields:
# $tmpobj->{"index"} = $1;
# $tmpobj->{"subindex"} = $2;
# $tmpobj->{"valueinfo"} = $3;
# $tmpobj->{"datatype"} = $4;
# $tmpobj->{"bitsize"} = $5;
# $tmpobj->{"access"} = $6;
# $tmpobj->{"defaultvalue"} = $7;
# $tmpobj->{"description"} = $8;

sub write_profile_section {
	my (@objects ) = @_;
	my $template = <<EOV ;
<Object>
	<Index>__INDEX</Index>
<Name>__NAME</Name>
<Type>__TYPE</Type>
<Bitsize>__BITSIZE</Bitsize>
<Info>
	<!-- repeat for each subitem -->
	<SubItem>
	<Name>__SUBNAME<Name>
	<Info>
		<DefaultData>__DEFAULTDATA</DefaultData>
	</Info>
	<SubItem>
</Info>
<Flags>
	<Access>__ACCESS</Access>
	<Category>__CATEGORY</Category>
</Flags>
</Object>
EOV

	printf "Template looks like this:\n";
	printf $template;


	foreach my $obj (@objects) {
		printf "<Object>\n";
		printf "\t<Index>".$obj->{"index"}."</Index>\n";
		printf "\t<Name>".$obj->{"description"}."</Name>\n";
		printf "\t<Type>".$obj->{"datatype"}."</Type>\n";
		printf "\t<Bitsize>".$obj->{"bitsize"}."</Bitsize>\n";
		printf "\t<Info>\n";
		printf "\t\t<DefaultData>".$obj->{"defaultvalue"}."</DefaultData>\n";
		printf "\t</Info>\n";
		printf "\t<Flags>\n";
		printf "\t\t<Access>".$obj->{"access"}."</Access>\n";
		printf "\t</Flags>\n";
		printf "</Object>\n";
	}
}

my $inputfile = "";

my $argc = $#ARGV+1;
printf "Number of arguments: $argc\n";
if ($argc < 1) {
	printf "Error, missing input file\n";
	printf "usage: $0 input\n";
	exit 1;
}

if (!defined $ARGV[0]) {
	printf("ARGV isn't set\n");
	exit 1;
}

print "read object dictionary from: $ARGV[0]\n";
my @objects = readod $ARGV[0];

#printthis @objects;

write_profile_section @objects;

