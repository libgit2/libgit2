#!/usr/bin/env ruby -W

require 'open-uri'

begin
  cipher_headers = %w(nibbles unknown openssl_name nist_name protocol kex au enc mac export).map(&:to_sym)
  Cipher = Struct.new("Cipher", *cipher_headers, :default) do
    def id
      Integer(nibbles.gsub(',0x', ''))
    end
  end
end

raise "Missing cipherlist argument" if ARGV.empty?
$default_list = ARGV[0].split(":")
# cipher_source = "https://raw.githubusercontent.com/drwetter/testssl.sh/2.9dev/etc/cipher-mapping.txt"
cipher_source = "cipher-mapping.txt"
$known_ciphers = []

open(cipher_source) do |file|
  file.each_line do |line|
    data = line.split(/\s/).reject {|c| c.empty? }
    raise unless data.count.between? 9, 10
    cipher = Cipher.new(*data)

    %w(openssl_name nist_name).each do |k|
      cipher.send("#{k}=", nil) if cipher.send(k) == "-"
    end

    $known_ciphers << cipher
  end
end

# Remap whatever cipher style we were passed to NIST
$default_list = $default_list.map do |name|
  known = $known_ciphers.find {|cipher| cipher.openssl_name == name or cipher.nist_name == name.gsub("-", "_") }
  raise "Unknown cipher #{name}" if known.nil?
  known.default = true
  known.nist_name
end

longest_name = $known_ciphers.reduce(0) {|memo, cipher| [(cipher.openssl_name.length + 2 rescue "NULL".length), memo].max }

backends = {
  mbedtls: {map_key: :id},
  openssl: {map_key: :openssl_name},
  secure_transport: {map_key: :id},
}
backends_by_mapped_key = {}
backends.each do |id, opts|
  backends_by_mapped_key[opts[:map_key]] ||= []
  backends_by_mapped_key[opts[:map_key]] << id
end

puts
puts "#define GIT_TLS_DEFAULT_CIPHERS \"#{$default_list.join(':')}\""
puts

backends_by_mapped_key.each_with_index do |data, index|
  map_key = data.shift
  defines = data.map {|bids| bids.map{|bid| "defined(GIT_#{bid.to_s.upcase})" } }.flatten

  puts "#{index == 0 ? "#if" : "#elif"} #{defines.join " || " }"
  puts

  puts "typedef struct {"
  puts "	uint32_t value;" if map_key == :id
  puts "	const char *openssl_name;" if map_key == :openssl_name
  puts "	const char *nist_name;"
  puts "} git_tls_cipher;"
  puts
end
puts "#endif"
puts

backends_by_mapped_key.each_with_index do |data, index|
  map_key = data.shift
  defines = data.map {|bids| bids.map{|bid| "defined(GIT_#{bid.to_s.upcase})" } }.flatten

  puts "#{index == 0 ? "#if" : "#elif"} #{defines.join " || " }"
  puts

  puts "static const git_tls_cipher git_tls_ciphers[] = {"
  $known_ciphers.sort_by {|c| c.id }.each do |cipher|
    unavailable = map_key == :openssl_name && !cipher.openssl_name || !cipher.nist_name

    openssl_name = cipher.openssl_name ? "\"#{cipher.openssl_name}\"" : "NULL"
    nist_name = (cipher.nist_name ? "\"#{cipher.nist_name}\"" : "NULL")
    padding = " " * (longest_name - openssl_name.length)
    comment = (unavailable ? ["/* ", " */"] : ["", ""])
    if map_key == :id
      puts "	%s{0x%04x, %s},%s" % [comment[0], cipher.id, nist_name, comment[1]]
    else
      puts "	%s{%s, %s%s},%s" % [comment[0], openssl_name, padding, nist_name, comment[1]]
    end
  end
  puts "};"
  puts
end
puts "#endif"
puts
