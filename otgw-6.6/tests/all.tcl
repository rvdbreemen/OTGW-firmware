package require Tcl 8.5 9

set dir [file dirname [file normalize [info script]]]
set root [file dirname $dir]

# tcltest stores the environment during the package require and restores that
# in cleanupTests. So any required environment setting must be done before.

# Find the latest version of the module libraries
set libs {
    libgpsim_otgw.so
    libgpsim_extras.so
    libgpsim_modules.so
}
if {[info exists env(LD_LIBRARY_PATH)]} {
    set libpath [split $env(LD_LIBRARY_PATH) :]
}
if {![catch {exec pkg-config --variable=libdir gpsim} libdir]} {
    lappend libpath $libdir
}
lappend libpath [file join $root chmodule/.libs] $root
lappend libpath /usr/local/lib /usr/local/lib64 /usr/lib /usr/lib64

set found {}
foreach path $libpath {
    foreach file [glob -nocomplain -directory $path -tails {*}$libs] {
	dict set found $file $path [file mtime [file join $path $file]]
    }
}

set libpath {}
dict for {file dict} $found {
    set last 0
    dict for {path mtime} $dict {
	if {$mtime > $last} {
	    set last $mtime
	    set choice $path
	}
    }
    if {$choice ni $libpath} {lappend libpath $choice}
}

set env(LD_LIBRARY_PATH) [join $libpath :]

package require tcltest 2.2

proc tcltest::GetMatchingFiles {args} {
    set path [testsDirectory]
    return [glob -nocomplain -directory $path -types {b c f p s} -- *.stc]
}

proc tcltest::runstctests {{command ""}} {
    variable testSingleFile
    variable numTestFiles
    variable numTests
    variable failFiles
    variable DefaultValue
    variable gpsim $command

    FillFilesExisted
    if {$gpsim eq ""} {
	global env
	if {[info exists env(GPSIM)]} {
	    set gpsim $env(GPSIM)
	} else {
	    set gpsim [auto_execok gpsim]
	}
    }

    set testSingleFile false

    puts [outputChannel] "Tests executed using:  $gpsim"
    puts [outputChannel] "Tests located in:  [testsDirectory]"
    puts [outputChannel] "Tests running in:  [workingDirectory]"
    puts [outputChannel] "Temporary files stored in [temporaryDirectory]"

    # [file system] first available in Tcl 8.4
    if {![catch {file system [testsDirectory]} result]
            && ([lindex $result 0] ne "native")} {
        # If we aren't running in the native filesystem, then we must
        # run the tests in a single process (via 'source'), because
        # trying to run then via a pipe will fail since the files don't
        # really exist.
        singleProcess 1
    }

    if {[singleProcess]} {
        # puts [outputChannel]  "Test files sourced into current interpreter"
    } else {
        # puts [outputChannel]  "Test files run in separate interpreters"
    }
    if {[llength [skip]] > 0} {
        puts [outputChannel] "Skipping tests that match:  [skip]"
    }
    puts [outputChannel] "Running tests that match:  [match]"

    if {[llength [skipFiles]] > 0} {
        puts [outputChannel]  "Skipping test files that match:  [skipFiles]"
    }
    if {[llength [matchFiles]] > 0} {
        puts [outputChannel]  "Only running test files that match:  [matchFiles]"
    }

    set timeCmd {clock format [clock seconds]}
    puts [outputChannel] "Tests began at [eval $timeCmd]"

    # Group the tests
    foreach file [lsort [GetMatchingFiles]] {
	set name [file rootname [file tail $file]]
	if {[regexp -- {^(.+)-([\d.]+)$} $name -> group num]} {
	    dict set tests $group.test $num $file
	}
    }

    # Filter the groups
    set tests [dict filter $tests key {*}[matchFiles]]
    set skip [dict filter $tests key {*}[skipFiles]]
    set tests [dict remove $tests {*}[dict keys $skip]]

    # Run each of the specified tests
    dict for {group dict} $tests {
	# Sort the tests within the group
	puts $group
	foreach num [lsort -command {package vcompare} [dict keys $dict]] {
	    if {[catch {
                incr numTestFiles
                runstc [dict get $dict $num]
            } msg]} {
                puts [outputChannel] "Test file error: $msg"
                # append the name of the test to a list to be reported
                # later
                lappend testFileFailures $file
            }
            if {$numTests(Failed) > 0} {
                set failFilesSet 1
            }
	}
	cleanupTests
    }

    # cleanup
    puts [outputChannel] "\nTests ended at [eval $timeCmd]"
    cleanupTests 1
    if {[info exists testFileFailures]} {
        puts [outputChannel] "\nTest files exiting with errors:  \n"
        foreach file $testFileFailures {
            puts [outputChannel] "  [file tail $file]\n"
        }
    }

    # Checking for subdirectories in which to run tests
    foreach directory [GetMatchingDirectories [testsDirectory]] {
        set dir [file tail $directory]
        puts [outputChannel] [string repeat ~ 44]
        puts [outputChannel] "$dir test began at [eval $timeCmd]\n"

        uplevel 1 [list ::source [file join $directory all.tcl]]

        set endTime [eval $timeCmd]
        puts [outputChannel] "\n$dir test ended at $endTime"
        puts [outputChannel] ""
        puts [outputChannel] [string repeat ~ 44]
    }
    return [expr {[info exists testFileFailures] || [info exists failFilesSet]}]
}

proc tcltest::runstc {file} {
    set name [file rootname [file tail $file]]
    set fd [open $file]
    set lines [split [read -nonewline $fd] \n]
    close $fd
    set descline [lsearch -regexp -inline $lines {^\s*test\s+(.*)}]
    if {![regexp {^\s*test\s+(.*)} $descline -> desc]} {
	set desc [lrange $descline 0 1]
    }
    set constraints {}
    foreach line [lsearch -regexp -inline -all $lines {^\s*constraint\s+(.*)}] {
	lappend constraints {*}[lrange $line 1 end]
    }
    set result [lsearch -regexp -inline -all $lines {^\s*(check|mark|eeprom|analyze)\s*(.*)}]
    test $name $desc -constraints $constraints -body [list gpsim $file] \
      -match gpsim -result [join $result \n]
}

proc tcltest::gpsim {file} {
    variable gpsim 
    # Send a "quit" command to stdin to make sure the test finishes
    set fd [open |[list {*}$gpsim -i $file << "quit\n"]]
    try {
	return [read -nonewline $fd]
    } finally {
	close $fd
    }
}

namespace eval tcltest::check {
    proc init {str} {
	variable parts {}
	variable part 0
	variable failures 0
	variable position 0

	set mark "\n### mark\n"
	set p 0
	while {[set x [set x [string first $mark $str $p]]] >= 0} {
	    lappend parts [string range $str $p [expr {$x - 1}]]
	    set p [expr {$x + [string length $mark]}]
	}
	lappend parts [string range $str $p end]
	variable data [lindex $parts 0]
    }

    proc linenum {pos} {
	variable parts
	variable part
	variable data

	set linenum 0
	for {set i 0} {$i < $part} {incr i} {
	    incr linenum [regexp -all "\n" [lindex $parts $i]]
	}
	incr linenum [regexp -all "\n" [string range $data 0 $pos]]
    }

    proc failed {} {
	variable failures
	incr failures
    }

    proc check {pattern args} {
	variable data
	variable position
	set rc [regexp -indices -start $position $pattern $data match submatch]
	if {$rc} {
	    lassign $match p1 p2
	    set position [expr {$p2 + 1}]
	    if {[lindex [regexp -about $pattern] 0]} {lassign $submatch p1 p2}
	    set str [string range $data $p1 $p2]
	    if {[llength $args] == 0} return
	} else {
	    set str ""
	}
	if {$str ni $args} {
	    if {$rc} {
		set linenum [regexp -all "\n" [string range $data 0 $position]]
		puts "Looking for: $pattern.\
		  Found: $str at line [incr linenum]. Expected: [join $args ,]"
	    } else {
		puts "Looking for: $pattern. No match found"
	    }
	    failed
	}
	return
    }

    proc mark {} {
	variable parts
	variable part
	variable data [lindex $parts [incr part]]
	variable position 0
    }

    proc eeprom {addr args} {
	variable data
	variable position
	set pattern {^=+  EEPROM  DUMP  =+$\n(.*)\n^={74}$}
	if {[regexp -lineanchor -start $position $pattern $data -> dump]} {
	    foreach line [split $dump \n] {
		if {[scan $line {%x: %n} start pos] != 2} continue
		lappend ee {*}[string range $line $pos [expr {$pos + 46}]]
	    }
	}
	foreach arg $args {
	    if {[string is integer -strict $arg]} {
		set expect [list $arg]
	    } elseif {[string is double -strict $arg]} {
		set arg [expr {round($arg * 256) + ($arg < 0 ? 65536 : 0)}]
		set expect [list [expr {$arg / 256}] [expr {$arg % 256}]]
	    } else {
		binary scan [encoding convertto utf-8] cu* expect
	    }
	    foreach value $expect {
		scan [lindex $ee $addr] %x val
		if {$val != $value} {
		    puts "EEPROM address $addr = $val, expected $value"
		    failed
		}
		incr addr
	    }
	}
	return
    }

    proc analyze {} {
	variable data
	foreach line [split $data \n] {
	    switch -glob $line {
		{[TB][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F]} {
		    incr msg([string index $line 0])
		}
		{Error 0[1-4]} {
		    puts "Error report: $line"
		    failed
		}
		{Thermostat report} {
		    set sent T
		    set rcvd B
		}
		{Boiler report} {
		    set rcvd T
		    set sent B
		}
		{Messages sent *} {
		    scan [string range $line 20 end] %d count
		    if {$msg($sent) != $count} {
			puts "Mismatch of sent messages: $msg($sent) of $count"
			failed
		    }
		}
		{Messages received *} {
		    scan [string range $line 20 end] %d count
		    if {$msg($rcvd) != $count} {
			puts "Mismatch of received messages: $msg($rcvd) of $count"
			failed
		    }
		}
		{Stop bit errors *} -
		{Parity errors *} -
		{Data bit errors *} -
		{Wrong direction *} -
		{Invalid messages *} -
		{Missing response *} -
		{DataID mismatch *} -
		{Multiple responses *} {
		    scan [string range $line 20 end] %d count
		    if {$count != 0} {
			puts "Error: [string trim [string range $line 0 19]]"
			failed
		    }
		}
	    }
	}
    }
}

proc tcltest::check {expect data} {
    check::init $data
    namespace eval check $expect
    return [expr {$check::failures == 0}]
}

namespace eval tcltest {
    namespace export runstctests
    customMatch gpsim [namespace which check]
}

namespace import tcltest::*

set setupsearch {}
if {[info exists env(GPSIM)]} {
    set gpsim $env(GPSIM)
} else {
    set gpsim [auto_execok gpsim]
}
if {[info exists env(PIC)]} {
    testConstraint $env(PIC) 1
    lappend setupsearch $env(PIC).stc
}
lappend setupsearch setup.stc
lassign [glob -directory $dir {*}$setupsearch] setup
lappend gpsim -L $dir -I $setup
#lappend gpsim -L $dir -I [file join $dir setup33.stc]
configure -verbose tpbe -singleproc 1 -testdir $dir {*}$argv
runstctests $gpsim
