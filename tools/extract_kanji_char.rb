#! /usr/bin/ruby

while line = gets
  line.each_char do |ch|
    puts ch if ch.match?(/\p{Han}/)
  end
end
