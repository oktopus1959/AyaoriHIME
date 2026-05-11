#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'
require 'optparse'

options = {
  base_score_file: 'work/wiki_hplt.all.score.0.5-0.1-10000.txt',
  realtime_pattern: 'work/input/realtime.ngram.*.txt',
  output_file: 'work/build/ngram-sys-source.txt',
  max_realtime_bonus: 5000,
  long_hiragana_base_cost: 6000,
  realtime_base_cost: 6000,
  realtime_geta_base_cost: 9500,
  user_ngram_file: 'work/input/user.ngram.*.txt',
  user_inflex_point: 25,
  bonus_factor: 250,
  user_base_cost: 6000,
  user_geta_base_cost: 9500
}

OptionParser.new do |opts|
  opts.on('--base FILE') { |v| options[:base_score_file] = v }
  opts.on('--realtime-pattern GLOB') { |v| options[:realtime_pattern] = v }
  opts.on('--output FILE') { |v| options[:output_file] = v }
  opts.on('--max-bonus N', Integer) { |v| options[:max_realtime_bonus] = v }
  opts.on('--base-cost N', Integer) { |v| options[:realtime_base_cost] = v }
  opts.on('--geta-base-cost N', Integer) { |v| options[:realtime_geta_base_cost] = v }
  opts.on('--user-ngram GLOB') { |v| options[:user_ngram_file] = v }
  opts.on('--user-inflex-point N', Integer) { |v| options[:user_inflex_point] = v }
  opts.on('--bonus-factor N', Integer) { |v| options[:bonus_factor] = v }
  opts.on('--user-base-cost N', Integer) { |v| options[:user_base_cost] = v }
  opts.on('--user-geta-base-cost N', Integer) { |v| options[:user_geta_base_cost] = v }
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

def starts_with_kanji?(text)
  /\A\p{Han}/.match?(text)
end

def hiragana_long?(text)
  text.length >= 4 && hiragana_only?(text)
end

def user_bonus(point, inflex_point, bonus_factor)
  value = point.to_f
  if value > inflex_point
    if value <= inflex_point * 2
      value = inflex_point + (value - inflex_point) * 0.4
    elsif value <= inflex_point * 4
      value = inflex_point * 1.4 + (value - inflex_point * 2) * 0.2
    elsif value <= inflex_point * 40
      value = inflex_point * 1.8 + (value - inflex_point * 4) * 0.1
    elsif value <= inflex_point * 400
      value = inflex_point * 2.16 + (value - inflex_point * 40) * 0.02
    else
      value = inflex_point * 9.36 + (value - inflex_point * 400) * 0.01
    end
  end
  (value * bonus_factor).to_i
end

def apply_bonus!(merged, base_entries, key, bonus, normal_base_cost, geta_base_cost, long_hiragana_base_cost, geta_char)
  if merged.key?(key)
    merged[key] -= bonus
    return :overwritten
  end

  base_cost =
    if !base_entries.include?(key) && hiragana_long?(key)
      long_hiragana_base_cost
    elsif key.start_with?(geta_char)
      geta_base_cost
    else
      normal_base_cost
    end
  merged[key] = base_cost - bonus
  :added
end

def register_user_entry!(user_entries, word, point, geta_char)
  user_entries[word] = point
  user_entries["#{geta_char}#{word}"] = point
  return unless word.length >= 6

  len = 5
  (0..(word.length - len)).each do |pos|
    user_entries[word[pos, len]] = point
  end
end

base_score_file = options[:base_score_file]
realtime_pattern = options[:realtime_pattern]
output_file = options[:output_file]
geta_char = "\u3013"

raise "base score file not found: #{base_score_file}" unless File.file?(base_score_file)

used_realtime_files = Dir.glob(realtime_pattern).sort.select { |path| File.file?(path) }
used_user_files = []
user_ngram_file = options[:user_ngram_file]
if user_ngram_file
  used_user_files = Dir.glob(user_ngram_file).sort.select { |path| File.file?(path) }
end

puts "[INFO] base file: #{base_score_file}"
puts "[INFO] realtime files: #{used_realtime_files.join(', ')}" unless used_realtime_files.empty?
puts "[INFO] user files: #{used_user_files.join(', ')}" unless used_user_files.empty?
puts "[INFO] output file: #{output_file}"

merged = {}
base_entries = {}
base_count = 0

File.foreach(base_score_file, chomp: true, encoding: 'UTF-8') do |line|
  next if line.strip.empty? || line.start_with?('#')

  key, cost_str = line.split("\t", 2)
  next if key.nil? || key.empty? || cost_str.nil?
  next unless /\A[+-]?\d+\z/.match?(cost_str)

  merged[key] = cost_str.to_i
  base_entries[key] = true
  base_count += 1
end

rt_counts = Hash.new(0)
max_realtime_count = 0
ignored_by_length = 0
malformed_realtime = 0
user_entries = {}
malformed_user = 0

used_realtime_files.each do |realtime_file|
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

    next if hiragana_only?(key) && len == 2

    count = count_str.to_i
    next if count <= 2 && len >= 4 && hiragana_only?(key)

    rt_counts[key] += count
  end
end

used_user_files.each do |user_file|
  File.foreach(user_file, chomp: true, encoding: 'UTF-8') do |line|
    stripped = line.strip
    next if stripped.empty? || stripped.start_with?('#')

    key, point_str = stripped.split(/[[:space:]]+/, 2)
    if key.nil? || key.empty?
      malformed_user += 1
      next
    end

    point = options[:user_inflex_point]
    unless point_str.nil? || point_str.empty?
      unless /\A[+-]?\d+\z/.match?(point_str)
        malformed_user += 1
        next
      end
      point = point_str.to_i
    end

    register_user_entry!(user_entries, key, point, geta_char)
  end
end

overwritten_count = 0
added_count = 0
user_overwritten_count = 0
user_added_count = 0

rt_counts.each_value do |count|
  max_realtime_count = count if count > max_realtime_count
end

rt_counts.each do |key, count|
  bonus = realtime_bonus(count, max_realtime_count, options[:max_realtime_bonus])
  next if bonus <= 0

  targets = [key]
  targets << "#{geta_char}#{key}" if starts_with_kanji?(key)

  targets.each do |target|
    case apply_bonus!(merged, base_entries, target, bonus, options[:realtime_base_cost], options[:realtime_geta_base_cost], options[:long_hiragana_base_cost], geta_char)
    when :overwritten
      overwritten_count += 1
    when :added
      added_count += 1
    end
  end
end

user_entries.each do |key, point|
  bonus = user_bonus(point, options[:user_inflex_point], options[:bonus_factor])
  case apply_bonus!(merged, base_entries, key, bonus, options[:user_base_cost], options[:user_geta_base_cost], options[:long_hiragana_base_cost], geta_char)
  when :overwritten
    user_overwritten_count += 1
  when :added
    user_added_count += 1
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

puts "[INFO] base=#{base_count} realtime_used=#{rt_counts.length} max_rt_count=#{max_realtime_count} overwritten=#{overwritten_count} added=#{added_count} ignored_len=#{ignored_by_length} malformed_rt=#{malformed_realtime} user_used=#{user_entries.length} malformed_user=#{malformed_user} user_overwritten=#{user_overwritten_count} user_added=#{user_added_count} out=#{output_file}"
