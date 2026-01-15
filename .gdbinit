set pagination off
set confirm off
set style enabled off

handle SIGSEGV stop print nopass

catch signal SIGSEGV
commands
  silent
  echo "\n* Segmentation fault detected *\n"
  backtrace
  quit 1
end

run

if $_exitcode == 0
  quit 0
else
  quit 1
end
