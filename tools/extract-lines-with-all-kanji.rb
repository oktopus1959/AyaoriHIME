#! /usr/bin/ruby

lines = []
extra = []
kanjiMap = {}

while line = gets
  next if line =~ /Lesson/ || line =~ /^\s*$/
  if line =~ /・.*・.*・/
    extra.push(line)
  else
    lines.push(line)
  end
end

def check_and_put_line(line, kanjiMap)
  kfound = false
  kfirst = false
  line.strip.split(//).each{|k|
    if k =~ /[一-龠]/
      kfound = true
      unless kanjiMap[k]
        kfirst = true
        kanjiMap[k] = true
      end
    end
  }
  if kfound && kfirst
    puts line
    puts
  end
end

rand(10).times do
  lines.push(lines.shift)
end

for n in (0...10)
  while n < lines.size
    check_and_put_line(lines[n], kanjiMap)
    n += 10
  end
end

rand(extra.size).times do
  extra.push(extra.shift)
end

extra.each{|line|
  check_and_put_line(line, kanjiMap)
}
