#!/usr/bin/perl

use strict;
use warnings;
use constant {
    TARGET_FILENAMES => [
        'logo.txt',
        'raw_silence.txt',
        'silence.txt',
        'scene.txt',
        'scene.txt.orig',
        'scene_filtered.txt',
        'scene_filtered.txt.orig',
        'scene_offset.txt',
        'index.html',
    ],
    TARGET_DIRNAMES => [
#        'thumbnails',
    ],
};

use File::Basename;
use File::Path;
use POSIX;

my $scene_filename = 'scene.txt';
my $output_dirname = shift @ARGV;

die "No such file. [$scene_filename]" unless -f $scene_filename;
die "Output directory already exists. [$output_dirname]" if -e $output_dirname;

File::Path::mkpath($output_dirname) or die;

for my $filename (@{TARGET_FILENAMES()}) {
    `cp "$filename" "$output_dirname/$filename"`;
}
for my $dirname (@{TARGET_DIRNAMES()}) {
    `cp -R "$dirname" "$output_dirname/$dirname"`;
}

open my $fh, '<', $scene_filename or die;
my @lines = <$fh>;
close $fh;

my $base_dump_dirname = "$output_dirname/dump";
for my $line (@lines) {
    my ($body_index, $start, $end, $target) = (split '\s+', $line)[0, 2, 3, 4];

    my $from_dirname = sprintf("dump/%03d", $body_index);
    my $to_dirname = "$output_dirname/$from_dirname";
    File::Path::mkpath($to_dirname);

    ($start, $end, $target) =
        map {POSIX::floor($_ * 2 + 0.1)} ($start, $end, $target);
    my $in1 = sprintf("%04d.png", 1);
    my $in2 = sprintf("%04d.png", $target - $start);
    my $in3 = sprintf("%04d.png", $target - $start + 1);
    my $in4 = sprintf("%04d.png", $end - $start + 2);

    `convert -resize 224x126 "$from_dirname/$in1" "$to_dirname/1.jpg"`;
    `convert -resize 224x126 "$from_dirname/$in2" "$to_dirname/2.jpg"`;
    `convert -resize 224x126 "$from_dirname/$in3" "$to_dirname/3.jpg"`;
    `convert -resize 224x126 "$from_dirname/$in4" "$to_dirname/4.jpg"`;
    `cp "$from_dirname/dump.mp4v" "$to_dirname"`;
}
