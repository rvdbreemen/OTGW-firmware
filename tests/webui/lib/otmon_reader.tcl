# TASK-1813: run OTmonitor 6.6's own log-file reader on one file.
# Loads the REAL procs fileanal and filedata from otmonitor.tcl
# (other-projects/otmonitor-6.6/otmonitor.vfs/otmonitor.tcl:1749-1842) and stubs only what they call
# for the GUI: connected, status, gui; process (otmonitor.tcl:467) prints each message it is handed.
# Usage: tclsh otmon_reader.tcl <otmonitor.tcl> <log file> <end time YYYYmmdd-HHMMSS>
# Output: "LAYOUT <fileanal result>", then one "MSG <wall clock YYYYmmdd-HHMMSS.uuuuuu> <message>" per
# message filedata passes on. The wall clock is formatted in the same (local) time zone filedata
# scans in, so it reads back what the file says.
set fh [open [lindex $argv 0]]; set src [read $fh]; close $fh
set lines [split $src \n]
foreach p {filedata fileanal} {
  set i [lsearch -regexp $lines "^proc $p "]
  if {$i < 0} { puts "ERROR proc $p not found"; exit 2 }
  set j [lsearch -start $i -exact $lines "\}"]
  eval [join [lrange $lines $i $j] \n]
}
proc connected {args} {}
proc status {args} {}
proc gui {args} {}
proc process {msg us} {
  set sec [expr {$us / 1000000}]
  puts "MSG [clock format $sec -format %Y%m%d-%H%M%S].[format %06d [expr {$us % 1000000}]] $msg"
}
set connected 1; set start 9999999999
set f [lindex $argv 1]
set dev [open $f]
puts "LAYOUT [list [fileanal]]"
close $dev
set dev [open $f]
coroutine c1 filedata [clock scan [lindex $argv 2] -format %Y%m%d-%H%M%S]
while {[info commands c1] ne ""} { update; after 1 }
close $dev
puts "END"
