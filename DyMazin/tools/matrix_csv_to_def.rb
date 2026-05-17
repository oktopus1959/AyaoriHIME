#!/usr/bin/env ruby
# frozen_string_literal: true

require 'csv'

def usage
  warn "使い方: ruby #{File.basename(__FILE__)} <matrix.def.csv> [output.def]"
  exit 1
end

usage if ARGV.empty?

input_path = ARGV[0]
output_path = ARGV[1]

unless File.file?(input_path)
  warn "入力ファイルが見つかりません: #{input_path}"
  exit 1
end

rows = []
right_count = nil

CSV.foreach(input_path, encoding: 'utf-8').with_index(1) do |row, line_number|
  if row.empty?
    warn "#{line_number} 行目が空です"
    exit 1
  end

  if right_count.nil?
    right_count = row.length
    if right_count <= 0
      warn "#{line_number} 行目の列数が不正です"
      exit 1
    end
  elsif row.length != right_count
    warn "#{line_number} 行目の列数が一致しません: #{row.length} != #{right_count}"
    exit 1
  end

  begin
    rows << row.map { |item| Integer(item, 10) }
  rescue ArgumentError
    warn "#{line_number} 行目の数値変換に失敗しました"
    exit 1
  end
end

if rows.empty?
  warn '入力ファイルが空です'
  exit 1
end

left_count = rows.length

output = output_path ? File.open(output_path, 'w:utf-8') : $stdout
begin
  output.puts "#{left_count} #{right_count}"
  rows.each_with_index do |row, left_id|
    row.each_with_index do |cost, right_id|
      output.puts "#{left_id} #{right_id} #{cost}"
    end
  end
ensure
  output.close if output_path
end
