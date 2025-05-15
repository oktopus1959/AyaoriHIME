while line = gets
  items = line.strip.split(/\s+/)
  
#  STDERR.puts "items: #{items.join('|')}"

  if items.size == 2
    cnt = items[1].to_i
    if cnt >= 50
      puts "#{items[0]}\t#{cnt - 30}"
      next
    end
  end
  puts line
end
