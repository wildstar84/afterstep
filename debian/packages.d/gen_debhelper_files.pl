#!/usr/bin/perl
# vim:ts=4:et:sts=4:sw=4:ai
use strict;
use warnings;

# Script reads debian/control to get list of the binary packages. Then
# for each package reads  or debian/packages.d/${package}.in
# it at the line which are equal to %filename%, where filename can be any file name.
#
# When called without "generate" argument, the text  between that line and the next %filename%
# are written to the file debian/package.filename
#
# When called with the "clean" argument it removes debian/package.filename files


die "Usage: $0 clean | generate valueofbuilddir valueoflibdir\n"
    if $#ARGV < 1 or ($ARGV[0] eq "clean" ? $#ARGV != 1 : $#ARGV != 2) ;
my $action      = $ARGV[0];
my $builddirval = $ARGV[1];
my $libdirval   = $ARGV[2];
my $cleanonly   = $action eq "clean";
my $skipdoc     = 1 if exists $ENV{'DEB_BUILD_OPTIONS'}
                       and $ENV{'DEB_BUILD_OPTIONS'} =~ /nodoc/;

die "Invalid action $action" unless $cleanonly or $action eq "generate";

my @debdirs = grep { -d $_ } ("./debian", "../debian", "../../debian");
die "Cannot find debian dir" unless $#debdirs < 1;
chdir "$debdirs[0]/.." or die "Cannot chdir to $debdirs[0]/..: $!\n";

my $dir="debian/packages.d";

# Don't use dh_listpackaes to make sure files for all packages are
# always generated, even if we build binary-arch or -indep only.
open CTRL, '<', "debian/control" or die "Cannot open debian/control: $!\n";
my @packages=grep { s/^Package:\s*// } <CTRL>;
close CTRL;
while (@packages)
{
    chomp (my $package = shift(@packages));
    my $fh = undef;
    my %dups;
    open IN, "<", "$dir/$package.in" or die "Cannot open $dir/$package.in: $!\n";
    while (<IN>)
    {
        next if /^\%#/;
        if (/^\%(.*)\%$/)
        {
            my $entry=$1;
            my $file="debian/$package.$entry";
            die "Duplicated entry '$entry' in $dir/$package.in:$.\n" if ++$dups{$file} > 1;
            unlink $file or die "Cannot unlink $file: $!\n" if -e $file;
            next if $cleanonly;

            close $fh if $fh;
            open $fh, ">", $file or die "Cannot open $file for writing: $!\n";
            chmod 0444, $file or die "Cannot chmod $file: $!\n";
            print $fh "### Generated from ${dir}/${package}.in ###\n";
            print "Generating $file\n";
        }
        else
        {
            next if $cleanonly;
            next if $skipdoc and m#^\s*usr/share/(doc|man)/#;
            s/#BUILD_TREE#/$builddirval/go;
            s/#LIBDIR#/$libdirval/go;
            print $fh $_;
        }
    }
    close $fh if $fh;
    close IN;
}
