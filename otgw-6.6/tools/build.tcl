#!/usr/bin/tclsh

if {"Tk" in [package names]} {wm withdraw .}

# set dir [file normalize [file join [file dirname [info script]] ..]]
set dir [pwd]

proc ctlvar {name format str} {
    global ctlvar
    if {$format eq ""} {
	dict set ctlvar $name $str
    } elseif {[scan $str $format val] == 1} {
	dict set ctlvar $name $val
    } else {
	error "Invalid value"
    }
}

interp create -safe ctl
ctl alias source ctlvar source %s
ctl alias version ctlvar version %f
ctl alias phase ctlvar phase {%[ab.]}
ctl alias patch ctlvar patch {%[ab0-9]}
ctl alias bugfix ctlvar bugfix %d
ctl alias build ctlvar build %d

set ctlvar {
    source	main.asm
    version	1.0
    phase	.
    patch	0
    bugfix	0
    build	0
}

set defines {
    version	1.0
    phase	.
    patch	0
    bugfix	0
}

ctl invokehidden source [file join $dir build.ctl]

set file [file join $dir [dict get $ctlvar source]]

if {![catch {open $file} f]} {
    # Get the first 4k of the source file
    set data [read $f 4096]
    # Use complete lines
    append data [gets $f]
    foreach line [lsearch -all -inline [split $data \n] {#define*}] {
	# Strip comments
	regsub {\s*;.*} $line {} line
	if {[regexp {#define\s+(\S+)\s+(.*)} $line -> name val]} {
	    if {[dict exists $defines $name]} {
		if {[string match {"*"} $val]} {
		    set val [string range $val 1 end-1]
		}
		dict set defines $name $val
	    }
	}
    }
    dict for {name val} $defines {
	if {$val ne [dict get $ctlvar $name]} {
	    dict set ctlvar build 0
	    dict set ctlvar $name $val
	}
    }
    close $f
}

# Increment the build number
dict incr ctlvar build

# Save the control information
set f [open [file join $dir build.ctl] w]
dict for {name val} $ctlvar {
    puts $f "$name\t$val"
}
close $f

set tstamp [clock format [clock seconds] -format "%H:%M %d-%m-%Y"]
set bld [dict get $ctlvar build]

set f [open [file join $dir build.asm] w]
puts $f "#define\tbuild\t\"$bld\""
puts $f "#define\ttstamp\t\"$tstamp\""
close $f

set version [dict get $ctlvar version]
if {[dict get $ctlvar phase] ne "." || [dict get $ctlvar patch] != 0} {
    append version [dict get $ctlvar phase] [dict get $ctlvar patch]
} 
if {[dict get $ctlvar bugfix] != 0} {
    append version . [dict get $ctlvar bugfix]
}

puts "Version: $version"
puts "Build revision: $bld"

exit
