#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'
require 'optparse'

options = {
  base_score_file: 'work/wiki_hplt.all.score.0.5-0.1-10000.txt',
  realtime_pattern: 'work/input/realtime.ngram.*.txt',
  output_file: 'work/build/ngram-sys-source.txt',
  max_realtime_bonus: 5000,
  realtime_base_cost: 6000,
  realtime_geta_base_cost: 9500
}

OptionParser.new do |opts|
  opts.on('--base FILE') { |v| options[:base_score_file] = v }
  opts.on('--realtime-pattern GLOB') { |v| options[:realtime_pattern] = v }
  opts.on('--output FILE') { |v| options[:output_file] = v }
  opts.on('--max-bonus N', Integer) { |v| options[:max_realtime_bonus] = v }
  opts.on('--base-cost N', Integer) { |v| options[:realtime_base_cost] = v }
  opts.on('--geta-base-cost N', Integer) { |v| options[:realtime_geta_base_cost] = v }
end.parse!(ARGV)

def realtime_bonus(count, max_count, max_bonus)
  return 0 if count <= 0 || max_count <= 0
  return max_bonus if count >= max_count

  normalized = Math.log(1.0 + count.to_f) / Math.log(1.0 + max_count.to_f)
  bonus = (normalized * max_bonus).round
  return 0 if bonus.negative?
  return max_bonus if bonus > max_bonus

  bonus
end

def hiragana_only?(text)
  /\A\p{Hiragana}+\z/.match?(text)
end

base_score_file = options[:base_score_file]
realtime_pattern = options[:realtime_pattern]
output_file = options[:output_file]
geta_char = "\u3013"

raise "base score file not found: #{base_score_file}" unless File.file?(base_score_file)

merged = {}
base_count = 0

File.foreach(base_score_file, chomp: true, encoding: 'UTF-8') do |line|
  next if line.strip.empty? || line.start_with?('#')

  key, cost_str = line.split("\t", 2)
  next if key.nil? || key.empty? || cost_str.nil?
  next unless /\A[+-]?\d+\z/.match?(cost_str)

  merged[key] = cost_str.to_i
  base_count += 1
end

rt_counts = Hash.new(0)
max_realtime_count = 0
ignored_by_length = 0
malformed_realtime = 0

Dir.glob(realtime_pattern).sort.each do |realtime_file|
  next unless File.file?(realtime_file)

  File.foreach(realtime_file, chomp: true, encoding: 'UTF-8') do |line|
    next if line.strip.empty? || line.start_with?('#')

    key, count_str = line.split("\t", 2)
    if key.nil? || key.empty? || count_str.nil? || !/\A[+-]?\d+\z/.match?(count_str)
      malformed_realtime += 1
      next
    end

    len = key.length
    if len < 2 || len > 4
      ignored_by_length += 1
      next
    end

    count = count_str.to_i
    #next if count <= 1 && len >= 4 && hiragana_only?(key)
    next if count <= 1

    rt_counts[key] += count
  end
end

overwritten_count = 0
added_count = 0

rt_counts.each_value do |count|
  max_realtime_count = count if count > max_realtime_count
end

rt_counts.each do |key, count|
  bonus = realtime_bonus(count, max_realtime_count, options[:max_realtime_bonus])
  next if bonus <= 0

  if merged.key?(key)
    merged[key] -= bonus
    overwritten_count += 1
  else
    base_cost = key.start_with?(geta_char) ? options[:realtime_geta_base_cost] : options[:realtime_base_cost]
    merged[key] = base_cost - bonus
    added_count += 1
  end
end

FileUtils.mkdir_p(File.dirname(output_file))
File.open(output_file, 'w:UTF-8') do |f|
  merged.each do |key, cost|
    f.write(key)
    f.write("\t")
    f.puts(cost)
  end
end

puts "[INFO] base=#{base_count} realtime_used=#{rt_counts.length} max_rt_count=#{max_realtime_count} overwritten=#{overwritten_count} added=#{added_count} ignored_len=#{ignored_by_length} malformed_rt=#{malformed_realtime} out=#{output_file}"
