#!/usr/bin/env ruby
# frozen_string_literal: true

require 'csv'

def usage
  warn "使い方: ruby #{File.basename(__FILE__)} <matrix.def> [output.csv]"
  exit 1
end

usage if ARGV.empty?

input_path = ARGV[0]
output_path = ARGV[1]

unless File.file?(input_path)
  warn "入力ファイルが見つかりません: #{input_path}"
  exit 1
end

File.open(input_path, 'r:utf-8') do |input|
  header = input.gets
  if header.nil?
    warn '入力ファイルが空です'
    exit 1
  end

  left_count, right_count = header.split.map(&:to_i)
  if left_count <= 0 || right_count <= 0
    warn "先頭行の形式が不正です: #{header.strip}"
    exit 1
  end

  rows = Array.new(left_count) { Array.new(right_count) }

  input.each_line.with_index(2) do |line, line_number|
    next if line.strip.empty?

    items = line.split
    if items.length != 3
      warn "#{line_number} 行目の形式が不正です: #{line.strip}"
      exit 1
    end

    left_id = Integer(items[0], 10)
    right_id = Integer(items[1], 10)
    cost = Integer(items[2], 10)

    unless left_id.between?(0, left_count - 1)
      warn "#{line_number} 行目の左接続識別子が範囲外です: #{left_id}"
      exit 1
    end

    unless right_id.between?(0, right_count - 1)
      warn "#{line_number} 行目の右接続識別子が範囲外です: #{right_id}"
      exit 1
    end

    if rows[left_id][right_id]
      warn "#{line_number} 行目で重複した組み合わせを検出しました: #{left_id} #{right_id}"
      exit 1
    end

    rows[left_id][right_id] = cost
  rescue ArgumentError
    warn "#{line_number} 行目の数値変換に失敗しました: #{line.strip}"
    exit 1
  end

  missing = []
  rows.each_with_index do |row, left_id|
    row.each_with_index do |cost, right_id|
      missing << [left_id, right_id] if cost.nil?
    end
  end

  unless missing.empty?
    left_id, right_id = missing.first
    warn "接続コストが不足しています: #{left_id} #{right_id}"
    exit 1
  end

  output = output_path ? File.open(output_path, 'w:utf-8') : $stdout
  begin
    rows.each do |row|
      output.write(CSV.generate_line(row))
    end
  ensure
    output.close if output_path
  end
end
