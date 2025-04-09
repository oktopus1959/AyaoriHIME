#! /usr/bin/ruby

lines = []
extra = []
kanjiMap = {}

while line = gets
  next if line =~ /Lesson/ || line =~ /^\s*$/
  if line =~ /・.{2,3}・.{2,3}・/
    extra.push(line)
  else
    lines.push(line)
  end
end

def check_and_put_line(line, kanjiMap)
  kfound = false
  kfirst = false
  kmap = {}
  line.strip.split(//).each{|k|
    if k =~ /[一-龠]/
      if kmap[k]
        kmap[k] += 1
      else
        kmap[k] = 1
      end
    end
  }
  if kmap.size > 0
    kj = kmap.max_by { |k, v| v }[0]
    unless kanjiMap[kj]
      kanjiMap[kj] = true
      puts line
      puts
    end
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

for n in (0...50)
  check_and_put_line(extra[n], kanjiMap)
end
