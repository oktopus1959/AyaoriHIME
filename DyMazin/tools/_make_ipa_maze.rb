#!/usr/bin/ruby -w

# 常用漢字表
JOYO_KANJI_PLUS = 'joyo-kanji-plus.txt'

# kanji-bigram
#KANJI_BIGRAM_FILE = 'kanji-bigram.top500.txt'
KANJI_BIGRAM_FILE = 'kanji-bigram.txt'

#$kanjiTableDir = '/c/Dev/Text/kanji-table'
$kanjiTableDir = 'c:/Dev/Text/kanji-table'
#$mazeCost = 5000
$mazeCost = 0
$verbose = false

while ARGV.length > 0 && ARGV[0] =~ /^-/
  arg = ARGV.shift
  if arg =~ /^-+[Vv]/
    $verbose = true
  elsif arg =~ /^-(d|-table-dir)/
    $kanjiTableDir = ARGV.shift
  end
end

$kanjiYomi = {}
$kanjiBigram = {}

def addYomiVariation(kanji, list)
  if list && list.length > 0
    tmpList = list.clone()
    list.each do |yomi|
      if yomi.length > 1 && yomi =~ /[きくちつ]$/
        tmpList.push(yomi[0...-1]+"っ")
      end
      if yomi.length > 0 && yomi =~ /^[かきくけこさしすせそたちつてと]/
        tmpList.push(yomi[0].tr("かきくけこさしすせそたちつてと", "がぎぐげござじずぜぞだぢづでど")+yomi[1..-1])
      elsif yomi.length > 0 && yomi =~ /^[はひふへほ]/
        tmpList.push(yomi[0].tr("はひふへほ", "ばびぶべぼ")+yomi[1..-1])
        tmpList.push(yomi[0].tr("はひふへほ", "ぱぴぷぺぽ")+yomi[1..-1])
      end
    end
    tmpMap = {}
    tmpList.each do |yomi|
      tmpMap[yomi] = 1
      if yomi.length > 1
        tmpMap[yomi.sub(/う$/,'い')] = 1
        tmpMap[yomi.sub(/く$/,'き')] = 1
        tmpMap[yomi.sub(/ぐ$/,'ぎ')] = 1
        tmpMap[yomi.sub(/す$/,'し')] = 1
        tmpMap[yomi.sub(/つ$/,'ち')] = 1
        tmpMap[yomi.sub(/ぬ$/,'に')] = 1
        tmpMap[yomi.sub(/ぶ$/,'び')] = 1
        tmpMap[yomi.sub(/む$/,'み')] = 1
        tmpMap[yomi.sub(/る$/,'り')] = 1
        tmpMap[yomi.sub(/きゃ$/,'か')] = 1
        tmpMap[yomi.sub(/ぎゃ$/,'が')] = 1
        tmpMap[yomi.sub(/しゃ$/,'さ')] = 1
        tmpMap[yomi.sub(/じゃ$/,'ざ')] = 1
        tmpMap[yomi.sub(/ちゃ$/,'た')] = 1
        tmpMap[yomi.sub(/ぢゃ$/,'だ')] = 1
        tmpMap[yomi.sub(/にゃ$/,'な')] = 1
        tmpMap[yomi.sub(/ひゃ$/,'は')] = 1
        tmpMap[yomi.sub(/びゃ$/,'ば')] = 1
        tmpMap[yomi.sub(/みゃ$/,'ま')] = 1
        tmpMap[yomi.sub(/りゃ$/,'ら')] = 1
        if yomi.length > 2
          tmpMap[yomi.sub(/える$/,'ゆ')] = 1
        end
      end
      tmpMap[yomi+"っ"] = 1
      if yomi.length > 3
        tmpMap[yomi[0...-1]] = 1
        tmpMap[yomi[0...-2]] = 1
        tmpMap[yomi[0...-3]] = 1
      elsif yomi.length == 3
        tmpMap[yomi[0...-1]] = 1
        tmpMap[yomi[0...-2]] = 1
      elsif yomi.length == 2
        tmpMap[yomi[0...-1]] = 1
      else
        tmpMap[yomi+"う"] = 1
        tmpMap[yomi+"し"] = 1
      end
    end
  end
  $kanjiYomi[kanji] = list.concat(tmpMap.keys - list).sort_by{|s| [-s.length, s]}.join('|')
end

File.readlines("#{$kanjiTableDir}/#{JOYO_KANJI_PLUS}").each do |line|
  next if line =~ /^#/
  items = line.strip.tr("ァ-ヶ", "ぁ-ゖ").split(/\s+/)
  addYomiVariation(items[0], items[1].split('|'))
end

STDERR.puts "read #{JOYO_KANJI_PLUS}: DONE" if $verbose

File.readlines("#{$kanjiTableDir}/#{KANJI_BIGRAM_FILE}").each do |line|
  next if line =~ /^#/
  items = line.strip.split(/\t/)
  if items.length == 2
    key = items[0]
    yomi = $kanjiBigram[key]
    if yomi
      yomi = yomi + "|" + items[1]
    else
      yomi = items[1]
    end
    $kanjiBigram[key] = yomi
  end
end

STDERR.puts "read #{KANJI_BIGRAM_FILE}: DONE" if $verbose

def kata2hira(s)
  s.tr("ァ-ヶ", "ぁ-ゖ")
end

def getPos(p1,p2,p3,p4,kt)
  if p1 == "名詞"
    if p3 == "地域"
      return "地名"
    elsif p3 == "人名"
      if p4 == "姓" || p4 == "名"
        return p4
      else
        return p3
      end
    elsif p3 == "組織"
      return p3
    elsif p2 == "固有名詞"
      return p2
    elsif p2 == "サ変接続"
      return "名詞サ変"
    elsif p2 == "形容動詞語幹"
      return "名詞形動"
    elsif p2 == "接尾"
      if p3 == "助数詞"
        return p3
      elsif p3 == "人名"
        return p2 + p3
      elsif p3 == "地域"
        return p2 + "地名"
      else
        return p2 + "一般"
      end
    else
      return p1
    end
  elsif p1 == "動詞"
    if kt =~ /五段・(.行)/
      return "動詞" + $1 + "五段"
    elsif kt == "一段"
      return "動詞一段"
    elsif kt =~ /^([カサザラ]変)/
      return "動詞" + $1
    else
      return nil  # 不正な動詞品詞
    end
  elsif p1 == "形容詞" || p1 == "副詞" || p1 == "連体詞" || p1 == "接続詞" || p1 == "感動詞"
    return p1
  elsif p1 == "接頭詞"
    return "接頭語"
  else
    return nil
  end
end

HIRAGANA_PATT = "[^ぁぃぅぇぉゃゅょんをー].{,3}?"

def find_yomi(yomi,h1,k1,h2,k2,h3,k3,h4,k4,r)
  p1 = $kanjiYomi[k1]
  p2 = $kanjiYomi[k2]

  return [] if !p1 && !p2

  STDERR.puts "find_yomi: kanji1=#{k1}:#{p1}" if $verbose
  STDERR.puts "find_yomi: kanji2=#{k2}:#{p2}" if $verbose
  STDERR.puts "find_yomi: kanji3=#{k3}:#{$kanjiYomi[k3]}" if $verbose && k3
  STDERR.puts "find_yomi: kanji4=#{k4}:#{$kanjiYomi[k4]}" if $verbose && k4

  # 漢字を1文字ずつ扱う(ここでは漢字4文字は対象外)
  if true # !k4 || k4.length == 0
    patt1 = ''

    if p1
      patt = "#{h1}((?:#{p1}))"
      patt1 = "#{h1}((?:#{p1}).{,1})"
    else
      patt = "#{h1}(#{HIRAGANA_PATT})"
      patt1 = patt
    end
    if p2
      patt = patt + "#{h2}((?:#{p2}))"
      patt1 = patt1 + "#{h2}((?:#{p2}).{,1})"
    else
      patt = patt + "#{h2}(#{HIRAGANA_PATT})"
      patt1 = patt1 + "#{h2}(#{HIRAGANA_PATT})"
    end

    if k3 && k3.length > 0
      p3 = $kanjiYomi[k3]
      if p3 || (p1 && p2)
        if p3
          patt = patt + "#{h3}((?:#{p3}))"
          patt1 = patt1 + "#{h3}((?:#{p3}).{,1})"
        else
          patt = patt + "#{h3}(#{HIRAGANA_PATT})"
          patt1 = patt1 + "#{h3}(#{HIRAGANA_PATT})"
        end
        if k4 && k4.length > 0
          p4 = $kanjiYomi[k4]
          if p4 || (p1 && p2 && p3)
            if p4
              patt = patt + "#{h4}((?:#{p4}))"
              patt1 = patt1 + "#{h4}((?:#{p4}).{,1})"
            else
              patt = patt + "#{h4}(#{HIRAGANA_PATT})"
              patt1 = patt1 + "#{h4}(#{HIRAGANA_PATT})"
            end
          end
        end
      end
    end
    patt = patt + r

    STDERR.puts "patt=#{patt}" if $verbose
    STDERR.puts "pat1t=#{patt1}" if $verbose

    if yomi =~ /^#{patt}$/
      STDERR.puts "find_yomi: result-1=#{[$1,$2,$3,$4].join(',')}" if $verbose
      return [$1,$2,$3,$4]
    end
    if yomi =~ /^#{patt1}$/
      STDERR.puts "find_yomi: result-2=#{[$1,$2,$3,$4].join(',')}" if $verbose
      return [$1,$2,$3,$4]
    end

    # 漢字が3文字までで、読みがマッチしないやつが1つだけのケース
    if p1 && p2 && (!k3 || k3.length == 0 || p3) && !k4
      # p1 をワイルドカードにしてみる
      patt = "#{h1}(#{HIRAGANA_PATT})"
      patt1 = patt
      patt = patt + "#{h2}((?:#{p2}))"
      patt1 = patt1 + "#{h2}((?:#{p2}).{,1})"
      if k3 && k3.length > 0
        patt = patt + "#{h3}((?:#{p3}))"
        patt1 = patt1 + "#{h3}((?:#{p3}).{,1})"
      end
      if yomi =~ /^#{patt}$/
        STDERR.puts "find_yomi: result-3=#{[$1,$2,$3,$4].join(',')}" if $verbose
        return [$1,$2,$3,$4]
      end
      if yomi =~ /^#{patt1}$/
        STDERR.puts "find_yomi: result-4=#{[$1,$2,$3,$4].join(',')}" if $verbose
        return [$1,$2,$3,$4]
      end
      # p2 をワイルドカードにしてみる
      patt = "#{h1}((?:#{p1}))"
      patt1 = "#{h1}((?:#{p1}).{,1})"
      patt = patt + "#{h2}(#{HIRAGANA_PATT})"
      patt1 = patt1 + "#{h2}(#{HIRAGANA_PATT})"
      if k3 && k3.length > 0
        patt = patt + "#{h3}((?:#{p3}))"
        patt1 = patt1 + "#{h3}((?:#{p3}).{,1})"
      end
      if yomi =~ /^#{patt}$/
        STDERR.puts "find_yomi: result-5=#{[$1,$2,$3,$4].join(',')}" if $verbose
        return [$1,$2,$3,$4]
      end
      if yomi =~ /^#{patt1}$/
        STDERR.puts "find_yomi: result-6=#{[$1,$2,$3,$4].join(',')}" if $verbose
        return [$1,$2,$3,$4]
      end
      # p3 をワイルドカードにしてみる
      if k3 && k3.length > 0
        patt = "#{h1}((?:#{p1}))"
        patt1 = "#{h1}((?:#{p1}).{,1})"
        patt = patt + "#{h2}((?:#{p2}))"
        patt1 = patt1 + "#{h2}((?:#{p2}).{,1})"
        patt = patt + "#{h2}(#{HIRAGANA_PATT})"
        patt1 = patt1 + "#{h2}(#{HIRAGANA_PATT})"
        if yomi =~ /^#{patt}$/
          STDERR.puts "find_yomi: result-7=#{[$1,$2,$3,$4].join(',')}" if $verbose
          return [$1,$2,$3,$4]
        end
        if yomi =~ /^#{patt1}$/
          STDERR.puts "find_yomi: result-8=#{[$1,$2,$3,$4].join(',')}" if $verbose
          return [$1,$2,$3,$4]
        end
      end
    end
  end

  # 漢字の2gramを扱う(ここでは漢字4文字もOK)
  if (!h1 || h1.length==0) && (!h2 || h2.length==0) && (!h3 || h3.length==0) && (!h4 || h4.length==0) && k3 && k3.length > 0
    if $kanjiBigram[k1+k2]
      # k1 と k2 が連続
      p1 = $kanjiBigram[k1+k2]
      p3 = $kanjiYomi[k3]
      p4 = $kanjiYomi[k4]

      STDERR.puts "find_yomi: stage-2: p1=<#{p1}>, k3=<#{k3}>, k4=<#{k4}>, p3=<#{p3 ? p3 : 'nil'}>, p4=<#{p4 ? p4 : 'nil'}>" if $verbose

      if p1
        patt = "(#{p1})"

        if p3
          patt = patt + "(#{p3})"
        else
          patt = patt + "(#{HIRAGANA_PATT})"
        end
        if k4 && k4.length > 0
          if p4
            patt = patt + "(#{p4})"
          elsif p3
            patt = patt + "(#{HIRAGANA_PATT})"
          else
            patt = nil
          end
        end
        if patt
          patt = patt + r

          if yomi =~ /^#{patt}$/
            STDERR.puts "find_yomi: result-9=#{[$1,'',$2,$3].join(',')}" if $verbose
            return [$1,'',$2,$3]
          end
        end
      end

      # p3,p4 をワイルドカードにしてみる
      if k3 && k3.length > 0
        if p3
          if yomi =~ /^(#{p1})(#{p3})(#{HIRAGANA_PATT})#{r}$/
            STDERR.puts "find_yomi: result-10B=#{[$1,'',$2,$3].join(',')}" if $verbose
            return [$1,'',$2,$3]
          end
        end
      end
      if k4 && k4.length > 0
#        if yomi =~ /^(#{p1})(#{p3})(#{HIRAGANA_PATT})#{r}$/
#          STDERR.puts "find_yomi: result-10B=#{[$1,'',$2,$3].join(',')}" if $verbose
#          return [$1,'',$2,$3]
#        end
        if p4
          if yomi =~ /^(#{p1})(#{HIRAGANA_PATT})(#{p4})#{r}$/
            STDERR.puts "find_yomi: result-11=#{[$1,'',$2,$3].join(',')}" if $verbose
            return [$1,'',$2,$3]
          end
        end
      end
      if k3 && k3.length > 0
        if yomi =~ /^(#{p1})(#{HIRAGANA_PATT})#{r}$/
          STDERR.puts "find_yomi: result-12=#{[$1,'',$2].join(',')}" if $verbose
          return [$1,'',$2]
        end
      end
    end

    if (!h3 || h3.length == 0) && $kanjiBigram[k2+k3]
      # k2 と k3 が連続
      p1 = $kanjiYomi[k1]
      p2 = $kanjiBigram[k2+k3]
      p4 = $kanjiYomi[k4]

      if p2
        if p1
          patt = "(#{p1})"
        else
          patt = "(#{HIRAGANA_PATT})"
        end
        patt = patt + "(#{p2})"
        if k4 && k4.length > 0
          if p4
            patt = patt + "(#{p4})"
          elsif p1
            patt = patt + "(#{HIRAGANA_PATT})"
          else
            patt = nil
          end
        end
        if patt
          patt = patt + r

          if yomi =~ /^#{patt}$/
            STDERR.puts "find_yomi: result-13=#{[$1,$2,'',$3].join(',')}" if $verbose
            return [$1,$2,'',$3]
          end
        end

        # p1,p4 をワイルドカードにしてみる
        if k4 && k4.length > 0
          if yomi =~ /^(#{p1})(#{p2})(#{HIRAGANA_PATT})#{r}$/
            STDERR.puts "find_yomi: result-14=#{[$1,$2,'',$3].join(',')}" if $verbose
            return [$1,$2,'',$3]
          end
          if p4
            if yomi =~ /^(#{HIRAGANA_PATT})(#{p2})(#{p4})#{r}$/
              STDERR.puts "find_yomi: result-15=#{[$1,$2,'',$3].join(',')}" if $verbose
              return [$1,$2,'',$3]
            end
          end
        else
          if yomi =~ /^(#{HIRAGANA_PATT})(#{p2})#{r}$/
            STDERR.puts "find_yomi: result-16=#{[$1,$2,''].join(',')}" if $verbose
            return [$1,$2,'']
          end
        end
      end
    end
    if k4 && k4.length > 0 && (!h4 || h4.length == 0) && $kanjiBigram[k3+k4]
      # k3 と k4 が連続
      p1 = $kanjiYomi[k1]
      p2 = $kanjiBigram[k2]
      p3 = $kanjiYomi[k3+k4]

      if p3
        if p1
          patt = patt + "(#{p1})"
        else
          patt = patt + "(#{HIRAGANA_PATT})"
        end
        if p2
          patt = patt + "(#{p2})"
        elsif p1
          patt = patt + "(#{HIRAGANA_PATT})"
        else
          patt = nil
        end
        if patt
          patt = patt + "(#{p3})" + r

          if yomi =~ /^#{patt}$/
              STDERR.puts "find_yomi: result-17=#{[$1,$2,$3,''].join(',')}" if $verbose
              return [$1,$2,$3,'']
          end
        end

        # p1,p2 をワイルドカードにしてみる
        if p1 || p2
          if p1
            if yomi =~ /^(#{p1})(#{HIRAGANA_PATT})(#{p3})#{r}$/
              STDERR.puts "find_yomi: result-18=#{[$1,$2,$3,''].join(',')}" if $verbose
              return [$1,$2,$3,'']
            end
          end
          if p2
            if yomi =~ /^(#{HIRAGANA_PATT})(#{p2})(#{p3})#{r}$/
              STDERR.puts "find_yomi: result-19=#{[$1,$2,$3,''].join(',')}" if $verbose
              return [$1,$2,$3,'']
            end
          end
        end
      end
    end
  end

  STDERR.puts "find_yomi: result-20=[]" if $verbose
  return []
end

# 交ぜ書きバリエーションの作成
def make_variation(yomi,h1,k1,h2,k2,h3,k3,h4,k4,r)
#  if k4 == "々"
#    k4 = k3
#  elsif k3 == "々"
#    k3 = k2
#  elsif k2 == "々"
#    k2 = k1
#  end
  STDERR.puts "make_variation: ENTER: yomi=#{yomi}, h1=#{h1}, k1=#{k1}, h2=#{h2}, k2=#{k2}, h3=#{h3}, k3=#{k3}, h4==#{h4}, k4=#{k4}, r=#{r}" if $verbose

  yomis = find_yomi(yomi,h1,k1,h2,k2=="々"?k1:k2,h3,k3==""?k2:k3,h4,k4==""?k3:k4,r)

  y1 = yomis[0]
  y2 = yomis[1]
  y3 = yomis[2]
  y4 = yomis[3]

  STDERR.puts "make_variation: after find_yomi: yomi=#{yomi}, yomis=[#{yomis.join(',')}], <#{k1}>=<#{y1}>, <#{k2}>=<#{y2 ? y2 : 'nil'}>, <#{k3}>=<#{y3 ? y3 : 'nil'}>, <#{k4}>=<#{y4 ? y4 : 'nil'}>" if $verbose

  result = {}

  if y4
    if y2.length == 0
      k1 = k1 + k2
      k2 = ''
    end
    if y3.length == 0
      k2 = k2 + k3
      k3 = ''
    end
    if y4.length == 0
      k3 = k3 + k4
      k4 = ''
    end
    STDERR.puts "make_variation 4: #{k1}=#{y1}, #{k2}=#{y2}, #{k3}=#{y3}, #{k4}=#{y4}" if $verbose
    result[h1+k1+h2+k2+h3+k3+h4+y4+r] = 1
    result[h1+y1+h2+k2+h3+k3+h4+y4+r] = 1
    result[h1+k1+h2+y2+h3+k3+h4+y4+r] = 1
    result[h1+y1+h2+y2+h3+k3+h4+y4+r] = 1
    result[h1+k1+h2+k2+h3+y3+h4+y4+r] = 1
    result[h1+y1+h2+k2+h3+y3+h4+y4+r] = 1
    result[h1+k1+h2+y2+h3+y3+h4+y4+r] = 1
    result[h1+y1+h2+k2+h3+k3+h4+k4+r] = 1
    result[h1+k1+h2+y2+h3+k3+h4+k4+r] = 1
    result[h1+y1+h2+y2+h3+k3+h4+k4+r] = 1
    result[h1+k1+h2+k2+h3+y3+h4+k4+r] = 1
    result[h1+y1+h2+k2+h3+y3+h4+k4+r] = 1
    result[h1+k1+h2+y2+h3+y3+h4+k4+r] = 1
    result[h1+y1+h2+y2+h3+y3+h4+k4+r] = 1
  elsif y3
    if y2.length == 0 && k2
      k1 = k1 + k2
      k2 = ''
    end
    if y3.length == 0 && k3
      k2 = k2 + k3
      k3 = ''
    end
    if k4
      k3 = k3 + k4
    end
    STDERR.puts "make_variation 3: #{k1}=#{y1}, #{k2}=#{y2}, #{k3}=#{y3}" if $verbose
    result[h1+k1+h2+k2+h3+y3+r] = 1
    result[h1+y1+h2+k2+h3+y3+r] = 1
    result[h1+k1+h2+y2+h3+y3+r] = 1
    result[h1+y1+h2+k2+h3+k3+r] = 1
    result[h1+k1+h2+y2+h3+k3+r] = 1
    result[h1+y1+h2+y2+h3+k3+r] = 1
  elsif y2
    STDERR.puts "make_variation 2: #{k1}=#{y1}, #{k2}=#{y2}" if $verbose
    result[h1+k1+h2+y2+r] = 1
    result[h1+y1+h2+k2+r] = 1
  end
  return result.keys.select{|x| x !~ /^[一-龠々]+$/}
end

$entries = {}

#def outputEntry(entry, comment)
#  ent = entry.sub(/名詞形動$/, "名詞")  # 同形の名詞形動と名詞は名詞形動だけを残す
#  unless $entries[ent]
#    $entries[entry] = 1
#    $entries[ent] = 1
#    puts "#{entry}\t#{comment}"
#  end
#end
def outputEntry(surf, base, items)
  puts "#{surf},#{items[1..9].join(',')},#{base},#{items[11]},#{items[12]}"
end

# 活用形の展開
$kformDefs = {
  "五段・カ行イ音便" => ["未然形,か,683", "未然ウ接続,こ,681", "連用形,き,689", "連用タ接続,い,687", "仮定形,け,675", "命令ｅ,せ,685", "仮定縮約１,きゃ,677"],
  "五段・サ行" => ["未然形,さ,733", "未然ウ接続,そ,732", "連用形,し,735", "仮定形,せ,729", "命令ｅ,せ,734", "仮定縮約１,しゃ,730"],
  "五段・マ行" => ["未然形,ま,764", "未然ウ接続,も,763", "連用形,み,767", "連用タ接続,ん,766", "仮定形,め,760", "命令ｅ,め,765", "仮定縮約１,みゃ,761"],
  "五段・ラ行" => ["未然形,ら,780", "未然形,ん,782", "未然ウ接続,ろ,778", "連用形,り,788", "連用タ接続,っ,786", "仮定形,れ,768",
                        "命令ｅ,れ,784", "仮定縮約１,りゃ,770", "体言接続特殊,ん,774", "体言接続特殊２,,776"],
  "五段・ワ行促音便" => ["未然形,わ,823", "未然ウ接続,お,820", "連用形,い,832", "連用タ接続,っ,829", "仮定形,え,814", "命令ｅ,え,826"],
  "一段" => ["未然形,,622", "未然ウ接続,よ,621", "連用形,,625", "仮定形,れ,617", "命令ｙｏ,よ,624", "命令ｒｏ,ろ,623", "仮定縮約１,りゃ,618", "体言接続特殊,ん,620"],
  "サ変・−スル" => ["基本形,する,583", "未然形,し,587", "未然ウ接続,しよ,585", "未然ウ接続,しょ,585", "未然レル接続,せ,586", "仮定形,すれ,581",
                        "命令ｙｏ,せよ,589", "命令ｒｏ,しろ,588", "仮定縮約１,すりゃ,582"],
  "形容詞・イ段" => ["文語基本形,,45", "未然ヌ接,から,47", "未然ウ接続,かろ,46", "連用タ接続,かっ,50", "連用テ接続,く,51", "連用テ接続,くっ,51", "連用ゴザイ接続,ゅう,49",
                     "体言接続,き,44", "仮定形,けれ,40", "命令ｅ,かれ,48", "仮定縮約１,けりゃ,41", "仮定縮約２,きゃ,42", "ガル接続,,39"],
  "形容詞・アウオ段" => ["文語基本形,し,23", "未然ヌ接,から,27", "未然ウ接続,かろ,25", "連用タ接続,かっ,33", "連用テ接続,く,35", "連用テ接続,くっ,35", "連用ゴザイ接続,う,31",
                     "体言接続,き,21", "仮定形,けれ,13", "命令ｅ,かれ,29", "仮定縮約１,けりゃ,15", "仮定縮約２,きゃ,17", "ガル接続,,11"],
}
def expand_kform(items)
  itemsList = []
  if items[0][0] == '*'
    surf = items[0][1..]
    items[0] = surf
    itemsList.push(items.clone)

    stem = surf[0...-1]
    yomiStem = items[11][0...-1]
    #STDERR.puts "stem=#{stem}" if $verbose
    ktype = items[8]
    defs = $kformDefs[ktype]
    if defs
      defs.each {|val|
        kfs = val.split(",")
        #STDERR.puts "kfs=#{kfs.join(",")}" if $verbose
        tail = kfs[1] ? kfs[1] : ""
        conn = kfs[2] ? kfs[2].to_i : items[1]
        items[0] = stem + tail    # 表層形
        items[1] = conn           # 左接続
        items[2] = conn           # 右接続
        items[9] = kfs[0]         # 活用形
        items[11] = yomiStem + tail.tr("ぁ-ゖ", "ァ-ヶ")  # 読み
        STDERR.puts "result=#{items.join(",")}" if $verbose
        itemsList.push(items.clone)
      }
    end
  else
    itemsList.push(items)
  end
  return itemsList
end

# 
# 語幹から基本形を得るための辞書
$mazeBaseMap = {}

def checkMazeBase(mazeYomi, ktype, kform)
    STDERR.puts "checkMazeBase: ENTER: mazeYomi=#{mazeYomi}, ktype=#{ktype}, kform=#{kform}"  if $verbose
    if kform == "基本形"
      if ktype.start_with?("サ変")
        mazeStem = mazeYomi[0...-2]
      else
        mazeStem = mazeYomi[0...-1]
      end
      STDERR.puts "mazeBaseMap[#{mazeStem}] = #{mazeYomi}"  if $verbose
      $mazeBaseMap[mazeStem] = mazeYomi
    end
end

def getMazeBase(mazeYomi)
  $mazeBaseMap.each {|stem, base|
    if mazeYomi.start_with?(stem)
      STDERR.puts "getMazeBase(#{mazeYomi}) => stem: #{stem}, base:#{base}"  if $verbose
      return base
    end
  }
  return mazeYomi
end

# main loop
def main(results, costs, items)

  surf = items[0]
  leftId = items[1]
  rightId = items[2]
  cost = items[3].to_i
  majorPos = items[4]
  minorPos = items[5]
#  pos1 = items[4]
#  pos2 = items[5]
#  pos3 = items[6]
#  pos4 = items[7]
  ktype = items[8]
  kform = items[9]
  base = items[10]
  hiraYomi = items[11].tr("ァ-ヶ", "ぁ-ゖ")

  hinshi = "#{majorPos}:#{minorPos}"

  #keyRest = "#{items[1..2].join(',')},#{items[4..9].join(',')}"
  keyRest = "#{leftId},#{rightId},#{hinshi}"

  # 元の定義を追加
  origKey = "#{surf},#{keyRest},#{base}"
  #origKey = "#{items[0]},#{keyRest}"
  origLine = "#{items[0..3].join(",")},#{base},#{hinshi},#{base}"
  STDERR.puts "main::check results[origKey=#{origKey}]=#{results[origKey]}, cost=#{cost}, costs[origKey]=#{costs[origKey]}"  if $verbose
  if !results[origKey] || cost < costs[origKey]
    STDERR.puts "main::put results[mazeKey=#{origKey}]=#{origLine}"  if $verbose
    results[origKey] = origLine
    costs[origKey] = cost
  end

  #STDERR.puts "surf=#{surf}" if $verbose

#  next if line =~ /^[^,]*[ァ-ヶ]/          # カタカナを含むやつは除外
  return unless surf =~ /^[ぁ-んァ-ヶー一-龠々]+$/  # ひらがな・カタカナ・漢字以外を含むやつは除外
  return unless surf =~ /[一-龠々]/                 # 漢字を含まないやつは除外

  # 交ぜ書きエントリーの追加処理
  # feature の末尾に "MAZE" を追加
  #mazeRest = "#{leftId},#{rightId},#{cost+$mazeCost},#{base},#{hinshi},#{surf},MAZE"
  mazeRestHead = "#{leftId},#{rightId},#{cost+$mazeCost}"
  mazeRestTail = "#{hinshi},#{base},#{surf},MAZE"

#  next if kf != '*' && kf != '基本形'

#  pos = getPos(pos1,pos2,pos3,pos4,kt)
#  next unless pos

  #outputEntry("#{surf}\t#{surf}\t#{pos}", "#{hiraYomi} -- BASE")

  if kform == "基本形"
    $mazeBaseMap = {}
  end

  if surf =~ /^([^一-龠々]*)([一-龠々])([^一-龠々]*)([一-龠々])(?:([^一-龠々]*)([一-龠々])(?:([^一-龠々]*)([一-龠々]))?)?(.*)$/
    # 2つ以上の漢字を含む
    make_variation(hiraYomi,$1,$2,$3,$4,$5,$6,$7,$8,$9).each do |mazeYomi|
      #outputEntry("#{mazeYomi}\t#{surf}\t#{pos}", "#{hiraYomi}") if mazeYomi =~ /[一-龠々]/
      #outputEntry(mazeYomi, surf, lcost, rcost, wcost, items)
      # 交ぜ書きの定義を追加(元の定義を上書きする可能性あり)
      checkMazeBase(mazeYomi, ktype, kform)
      mazeKey = "#{mazeYomi},#{keyRest},#{surf}"
      #mazeKey = "#{mazeYomi},#{keyRest}"
      if $verbose
        STDERR.puts "main::check results[mazeKey=#{mazeKey}]=#{results[mazeKey]}, cost=#{cost}, costs[mazeKey]=#{costs[mazeKey]}"  if $verbose
      end
      if !results[mazeKey] || cost < costs[mazeKey]
        mazeLine = "#{mazeYomi},#{mazeRestHead},#{getMazeBase(mazeYomi)},#{mazeRestTail}"
        STDERR.puts "main::put results[mazeKey=#{mazeKey}]=#{mazeLine}"  if $verbose
        results[mazeKey] = mazeLine
        costs[mazeKey] = cost
      end
    end
  end
  # 表層が漢字を含み、ひらがなだけの見出し語
  if surf =~ /[一-龠々]/
    #outputEntry(hiraYomi, surf, items)
    # 交ぜ書きの定義を追加(元の定義を上書きする可能性あり)
    checkMazeBase(hiraYomi, ktype, kform)
    mazeKey = "#{hiraYomi},#{keyRest},#{surf}"
    #mazeKey = "#{hiraYomi},#{keyRest}"
    if $verbose
      STDERR.puts "main::check results[mazeKey=#{mazeKey}]=#{results[mazeKey]}, cost=#{cost}, costs[mazeKey]=#{costs[mazeKey]}"  if $verbose
    end
    if !results[mazeKey] || cost < costs[mazeKey]
      mazeLine = "#{hiraYomi},#{mazeRestHead},#{getMazeBase(hiraYomi)},#{mazeRestTail}"
      STDERR.puts "main::put results[#{mazeKey}]=(hiraYomi)#{mazeLine}"  if $verbose
      results[mazeKey] = mazeLine
      costs[mazeKey] = cost
    end
  end
end

# main
results = {}
costs = {}
while line = gets
  STDERR.puts "line: " + line if $verbose

  items = line.strip.split(',')

  next if !items[0] || !items[0][0] || items[0][0] == '#'
  next if items[0].size > 8 && (items[0] =~ /[ァ-ヶ].*[ぁ-ゖ一-龠]|[ぁ-ゖ一-龠].*[ァ-ヶ]/)  # カタカナ含んで長いやつは削除

  if items.size < 11
    if items.size >= 5
      puts items[0..4].join(",")
    end
    next
  end

  expand_kform(items).each {|feats|
    main(results, costs, feats)
  }

end

# 出力
results.values.each {|line|
  puts line
}

