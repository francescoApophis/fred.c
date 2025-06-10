-- ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
--
-- this script generates a folder:
--
--       fred_test_[n]/             -> it can either be a custom name or 'fred_test_n' (ordered numerically by 'n').
--          |- keys.txt             -> keys to be fed to Fred, in ascii-int form.
--          |- keys_readable.txt    -> the same keys but in ascii form. Optional, is more for visual debugging. 
--          |- seed.txt             -> Random seed used by math.randomseed() to generate the current test.
--          |- output.txt           -> contains the state of the buffer after the last key was fed to Fred.
--                                     This can be used for a faster test (but that doesn't give much info about failures)
--          |- snaps.txt            -> contains the state of the buffer (snap) after each key got fed.  
--                                     This is used for a much in depth (but slower) test.
--                                     NOTE: in the file, snaps are called 'snapshot' because test.c will search for 
--                                     this name to find the start of one, since it's more unlikely 
--                                     for this script to generate 'snapshot' than 'snap' somewhere
--                                     
--
-- ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



-- ****************** helpers ******************
local fmt       = string.format
local rand      = math.random
local get_text  = vim.api.nvim_buf_get_text
local set_text  = vim.api.nvim_buf_set_text
local set_lines = vim.api.nvim_buf_set_lines
local get_lines = vim.api.nvim_buf_get_lines
local rand_bool = function() return ((rand(1,2) - 1) == 1 and true) or false end
local _print    = function(i) print((type(i) ~= 'table' and i) or vim.inspect(i)) end


local assert_type = function(var, expected_types)
  local pretty_types = vim.inspect(expected_types):gsub('[{}]', ''):gsub(',%s("%w+")%s$', ' or %1')

  for _, _type in ipairs(expected_types) do
    if type(var) == _type then
      if _type == 'string' and var == '' then
        error(fmt('invalid value: expected%s, got %q', pretty_types, 'empty string'))
      end
      return var
    end
  end
  error(fmt('invalid value: expected%s, got %q', pretty_types, type(var)))
end


local line_len = function(bufnr, row)
  if row == 0 then return #(get_lines(bufnr, 0, 1, false)[1]) end
  return #(get_lines(bufnr, row - 1, row, false)[1])
end

local get_tot_lines = function(bufnr)
  local l = get_lines(bufnr, 0, -1, true)
  if l[1] == '' and l[2] == nil then 
    return 0 
  end
  return #l
end


local ASCII = {}

ASCII.i2a = {
  [9]   = '\t',
  [10]  = '\n',
  [32]  = ' ',
  [33]  = '!',
  [34]  = '"',
  [35]  = '#',
  [36]  = '$',
  [37]  = '%',
  [38]  = '&',
  [39]  = '\'',
  [40]  = '(',
  [41]  = ')',
  [42]  = '*',
  [43]  = '+',
  [44]  = ',',
  [45]  = '-',
  [46]  = '.',
  [47]  = '/',
  [48]  = '0',
  [49]  = '1',
  [50]  = '2',
  [51]  = '3',
  [52]  = '4',
  [53]  = '5',
  [54]  = '6',
  [55]  = '7',
  [56]  = '8',
  [57]  = '9',
  [58]  = ':',
  [59]  = ';',
  [60]  = '<',
  [61]  = '=',
  [62]  = '>',
  [63]  = '?',
  [64]  = '@',
  [65]  = 'A',
  [66]  = 'B',
  [67]  = 'C',
  [68]  = 'D',
  [69]  = 'E',
  [70]  = 'F',
  [71]  = 'G',
  [72]  = 'H',
  [73]  = 'I',
  [74]  = 'J',
  [75]  = 'K',
  [76]  = 'L',
  [77]  = 'M',
  [78]  = 'N',
  [79]  = 'O',
  [80]  = 'P',
  [81]  = 'Q',
  [82]  = 'R',
  [83]  = 'S',
  [84]  = 'T',
  [85]  = 'U',
  [86]  = 'V',
  [87]  = 'W',
  [88]  = 'X',
  [89]  = 'Y',
  [90]  = 'Z',
  [91]  = '[',
  [92]  = '\\',
  [93]  = ']',
  [94]  = '^',
  [95]  = '_',
  [96]  = '`',
  [97]  = 'a',
  [98]  = 'b',
  [99]  = 'c',
  [100] = 'd',
  [101] = 'e',
  [102] = 'f',
  [103] = 'g',
  [104] = 'h',
  [105] = 'i',
  [106] = 'j',
  [107] = 'k',
  [108] = 'l',
  [109] = 'm',
  [110] = 'n',
  [111] = 'o',
  [112] = 'p',
  [113] = 'q',
  [114] = 'r',
  [115] = 's',
  [116] = 't',
  [117] = 'u',
  [118] = 'v',
  [119] = 'w',
  [120] = 'x',
  [121] = 'y',
  [122] = 'z',
  [123] = '{',
  [124] = '|',
  [125] = '}',
  [126] = '~',
}


ASCII.a2i = {}
for k, v in pairs(ASCII.i2a) do 
  ASCII.a2i[v] = k
end



-- ******************     ******************



local FRED_MAPPINGS = {
  ['escape'] = 27,
  ['delete'] = 127,
  ['up']     = ASCII.a2i['k'],
  ['down']   = ASCII.a2i['j'],
  ['left']   = ASCII.a2i['h'],
  ['right']  = ASCII.a2i['l'],
  ['insert'] = ASCII.a2i['i'],
}


local FILENAMES = {
  fred_output = '/fred_output.txt', -- NOTE+TODO: this is only temporary as fred_editor_init() needs an existent file to be initialized
  output      = '/output.txt',
  keys        = '/keys.txt',
  readable    = '/keys_readable.txt',
  snaps       = '/snaps.txt',
  seed        = '/seed.txt'
}


---@param path                  string
---@param seed                  number The current seed used by math.randomseed(); it will be written into its own file.
---@param max_places_to_edit    number
---@param max_edits_in_insert   number
---@return number Returns bufnr of the to-be-edited buffer
local function set_files_and_buf(path, seed, max_places_to_edit, max_edits_in_insert)
  vim.api.nvim_exec2('!mkdir -p ' .. path, {})

  io.open(path .. FILENAMES.snaps, 'w'):close() -- clear previous content
  io.open(path .. FILENAMES.output, 'w'):close()

  local seed_file = io.open(path .. FILENAMES.seed, 'w')
  -- seed_file:write(tostring(seed))
  seed_file:write(fmt('seed: %d\nmax_places_to_edit: %d\nmax_edits_in_insert: %d\n', seed, max_places_to_edit, max_edits_in_insert))
  seed_file:close()

  local bufnr = vim.fn.bufadd(path .. FILENAMES.output)
  vim.fn.bufload(bufnr)
  set_lines(bufnr, 0, -1, false, {}) -- clear buffer if file is already open in a file
  return bufnr
end



---@param feed      number[]
---@param path      string
---@param readable  boolean
local function write_keys2file(feed, path, readable)
  local str = ''

  if readable then
    for _, v in ipairs(feed) do
      if     v == 10  then str = str .. 'NEWLINE'
      elseif v == 9   then str = str .. 'TAB'
      elseif v == 27  then str = str .. 'ESC'
      elseif v == 127 then str = str .. 'BACKSPACE'
      else                 str = str .. ASCII.i2a[v]
      end
      str = str .. '\n'
    end

    local f = io.open(path .. FILENAMES.readable, 'w')
    f:write(str)
    f:close()
  else
    for _, v in ipairs(feed) do
      str = str .. tostring(v) .. '\n'
    end
    local f = io.open(path .. FILENAMES.keys, 'w')
    f:write(str)
    f:close()
  end
end



---@return number ascii-integer
local function get_rand_ikey()
  return (rand(20) % 2  == 0 and rand(32, 126)) or 
         (rand(20) % 4  == 0 and 127) or 
         (rand(20) % 14 == 0 and 9) or 
         (rand(20) % 8  == 0 and 10) or rand(32, 126)
         -- (rand(20) % 12 == 0 and 10) or rand(32, 126)
end


---@param bufnr     number  Number of the to-be-edited buffer
---@param curs_row  number 
---@param curs_col  number 
---@param key       number 
---@return number, number   Returns the updated curs_row and curs_col 
local function text_at(bufnr, curs_row, curs_col, key)
  assert(type(key) == 'number', fmt('invalid "key", expected "number" got %q', type(key)))

  if key == 127 then
    if curs_row == 0 and curs_col == 0 then  -- NOTE: doing this just for clarity, I know it's useless
      return 0, 0
    elseif curs_row > 0 and curs_col == 0 then 
      local curr_line = get_lines(bufnr, curs_row, curs_row + 1, true)
      local line_up_len = #get_lines(bufnr, curs_row - 1, curs_row, true)[1]
      set_lines(bufnr, curs_row, curs_row + 1, true, {})
      set_text(bufnr, curs_row - 1, line_up_len, curs_row - 1, line_up_len , curr_line)
      return curs_row - 1, line_up_len
    else
      set_text(bufnr, curs_row, curs_col - 1, curs_row, curs_col, {})
      return curs_row, curs_col - 1
    end 

  elseif key == 10 then 
    local curr_line_len = #get_lines(bufnr, curs_row, curs_row + 1, true)[1]
    if curs_col == curr_line_len then
      set_lines(bufnr, curs_row + 1, curs_row + 1, true, {''})
    else
      local start_to_col = get_text(bufnr, curs_row, 0, curs_row, curs_col, {})[1]
      local col_to_end = get_text(bufnr, curs_row, curs_col, curs_row, curr_line_len + 1, {})[1]
      set_lines(bufnr, curs_row, curs_row + 1, true, {start_to_col, col_to_end})
    end
    return curs_row + 1, 0
  else
    set_text(bufnr, curs_row, curs_col, curs_row, curs_col, {ASCII.i2a[key]})
    return curs_row, curs_col + 1
  end
end



---@param bufnr           number Number of the to-be-edited buffer
---@param snaps           string 
---@param snap_num        number
---@param key_inserted    number 
---@param curs_row        number Cursor row after key got inserted 
---@param curs_col        number Cursor col after key got inserted
---@return string snap 
local function take_snap(bufnr, snaps, snap_num, key_inserted, curs_row, curs_col)
  assert_type(curs_row, {"number"})
  assert_type(curs_col, {"number"})
  -- NOTE: snaps are written as 'snapshot' because test.c will search for 
  -- this name to find the start of one, since it's more unlikely 
  -- for the script to generate 'snapshot' than 'snap' somewhere
  -- NOTE: this is shown basically only for visual debugging purposes
  local key_inserted = (key_inserted == 9   and 'TAB') or 
                       (key_inserted == 27  and 'ESC') or 
                       (key_inserted == 10  and 'NEWLINE') or 
                       (key_inserted == 127 and 'BACKSPACE') or ASCII.i2a[key_inserted]
  local lines = get_lines(bufnr, 0, -1, false)
  local snap  = fmt('\n[snapshot: %d, inserted: %q, %d:%d]\n%s', 
                    snap_num,
                    key_inserted, 
                    curs_row + 1, curs_col + 1,
                    table.concat(lines, '\n'))
  snaps = snaps .. snap 
  return snaps
end




---@param bufnr        number   Number of the to-be-edited buffer
---@param feed         number[]
---@param curs_row     number
---@param curs_col     number
---@param max_keys     number
---@param snaps        string
---@return number, number, string, number   Returns the updated curs_row and curs_col, 
--                                          the updated snaps and the number of 
--                                          taken snaps -- TODO: better desc
local function insert_rand_ikeys(bufnr, feed, curs_row, curs_col, max_keys, snaps, snap_num)
  -- TODO: change name of max_keys 
  assert(type(curs_row) == 'number' and type(curs_col) == 'number', 
    fmt('received invalid cursor value type: curs_row (%s), curs_col (%s)', type(curs_row), type(curs_col)))

  table.insert(feed, ASCII.a2i['i'])
  local n = rand(max_keys) 

  local maybe_mass_delete = rand(20) % 12 == 0
  for i=1,n do
    local key = (maybe_mass_delete and 127) or get_rand_ikey()
    table.insert(feed, key)
    
    local prev_curs_row, prev_curs_col = curs_row, curs_col
    curs_row, curs_col = text_at(bufnr, curs_row, curs_col, key)

    snap_num = snap_num + 1
    snaps = take_snap(bufnr, snaps, snap_num, key, prev_curs_row, prev_curs_col)

    assert(type(curs_row) == 'number' and 
           type(curs_col) == 'number', 
          fmt('received invalid cursor value type: curs_row (%s), curs_col (%s)', type(curs_row), type(curs_col)))
  end

  table.insert(feed, 27) -- ESC
  return curs_row, curs_col, snaps, snap_num
end


---@param feed     number[] Array of ascii-ints to be fed to Fred
---@param bufnr    number   Number of the to-be-edited buffer
---@param prev_row number   
---@param prev_col number   
---@param next_row number   
---@param next_col number   
---@return nil
local function gen_keys_to_next_curs_pos(feed, bufnr, prev_row, prev_col, next_row, next_col)

  local function gen(prev, next, dir_a, dir_b)
    assert_type(prev,  {'number'})
    assert_type(next,  {'number'})
    assert_type(dir_a, {'string'})
    assert_type(dir_b, {'string'})

    -- local diff = (next > prev and next - prev) or (next < prev and prev - next) or 0
    local diff = (next > prev and next - prev) or prev - next

    if diff > 0 then
      local dir = (next < prev and dir_a) or dir_b 
      for i=1,diff do
        table.insert(feed, FRED_MAPPINGS[dir])
      end
    end
  end

  gen(prev_row, next_row, 'up', 'down')

  -- NOTE: when scrolling in fred, if next-line-len > cursor-col,
  -- we set cursor-col to be next-line-len. Here we're doing the same
  -- thing so we can generate the right hor-keys to reach the edit destination
  if next_row ~= prev_row then
    local diff = (next_row > prev_row and next_row - prev_row) or (prev_row - next_row)
    for i=1,diff-1 do 
      local line_at_row = nil
      if next_row > prev_row then
        line_at_row = get_lines(bufnr, prev_row + i, prev_row + i + 1, true)[1]
      else
        line_at_row = get_lines(bufnr, prev_row - i, prev_row - i + 1, true)[1]
      end
      if prev_col > #line_at_row then
        prev_col = #line_at_row
      end
    end
  end

  local line_at_row = get_lines(bufnr, next_row, next_row + 1, true)[1]
  prev_col = (prev_col > #line_at_row and #line_at_row) or prev_col
  gen(prev_col, next_col, 'left', 'right')

end




-- desc:
-- Edit buffer at 'bufnr' by generating: a random position in the buffer, the 
-- motion keys to reach it, and random insert-mode-keys characters to be inserted in
-- the buffer. All these keys are saved in 'feed' array as ASCII-int, and will 
-- be fed to Fred through 'test.c'.
--
---@param bufnr                   number   Number of the to-be-edited buffer
---@param feed                    number[] Array of ascii-ints to be fed to Fred
---@param curs_row                number      
---@param curs_col                number
---@param max_places_to_edit      number 
---@param max_edits_in_insert     number
---@param snaps                   string
---@return nil
local function edit_buffer(bufnr, feed, curs_row, curs_col, max_places_to_edit, max_edits_in_insert, snaps)
  local snap_num = 0
  curs_row, 
  curs_col, 
  snaps, 
  snap_num = insert_rand_ikeys(bufnr, feed, curs_row, curs_col, max_edits_in_insert, snaps, snap_num)

  for i=1, max_places_to_edit do
    local tot_lines = get_tot_lines(bufnr)
    local next_curs_row = rand(0, (tot_lines > 0 and tot_lines - 1) or 0)
    local next_curs_col = rand(0, #get_lines(bufnr, next_curs_row, next_curs_row + 1, true)[1])
    gen_keys_to_next_curs_pos(feed, bufnr, curs_row, curs_col, next_curs_row, next_curs_col)

    curs_row,
    curs_col,
    snaps,
    snap_num = insert_rand_ikeys(bufnr, feed, next_curs_row, next_curs_col, max_edits_in_insert, snaps, snap_num)
  end

  return snaps
end



-- desc: 
-- Generate names like 'fred_test_1', 'fred_test_2', ...,
-- where 'fred_test_' is the default name.
-- The number starts from the greatest 'fred_test_' found in the folder.
---@return nil
local function get_default_test_name()
  local default_name = 'fred_test_'

  local cmd = fmt('!find . -type d -name "%s*"', default_name)
  local out = vim.api.nvim_exec2(cmd, {output = true}).output:sub(#cmd + 5, -2)
  local dir_names = {}

  if #out == 0 then
    return default_name .. '1'
  end
  
  for name in out:gmatch('[^\n]+') do
    table.insert(dir_names, name)
  end

  table.sort(dir_names, function(a, b)
    local a_num, b_num = a:match('%d+'), b:match('%d+')
    a_num = (type(a_num) == 'nil' and -1000) or tonumber(a_num)
    b_num = (type(b_num) == 'nil' and -1000) or tonumber(b_num)
    return a_num < b_num 
  end)

  -- in case there are no numbered 'fred_test' folders like 'fred_test_delete' etc.
  local last_test_num_str = dir_names[#dir_names]:match('%d+')
  local next_test_num = (last_test_num_str == nil and 0) or tonumber(last_test_num_str)
  return default_name .. tostring(next_test_num + 1)
end



---@param test_name               (string | nil)  (Optional) Name for the folder containing the files; 
--                                                if nil, 'fred_test_1', 'fred_test_2', ... will be used instead. 
--                                                The number starts from the greatest 'fred_test_' found in the folder.
---@param seed                    (number | nil)  (Optional) Seed for math.randomseed(); if nil, os.time() will be used instead.
---@param gen_keys_readable_file  (boolean | nil) (Optional) Generate a 'keys.txt' with chars converted back to ASCII.
---@param max_places_to_edit      (number | nil)  (Optional) 
---@param max_edits_in_insert     (number | nil)  (Optional) Max num of keys to be inserted in insert mode; 
---@return nil
local function gen_test(args)
  local test_name              = assert_type(args.test_name, {'string', 'nil'}) or get_default_test_name()
  local seed                   = assert_type(args.seed, {'number', 'nil'}) or os.time()
  local gen_keys_readable_file = assert_type(args.gen_keys_readable_file, {'boolean', 'nil'}) or false
  local max_places_to_edit     = assert_type(args.max_places_to_edit, {'number', 'nil'}) or 10
  local max_edits_in_insert    = assert_type(args.max_edits_in_insert, {'number', 'nil'}) or 3

  math.randomseed(seed)
  vim.notify(fmt('Generating Fred-test: %s, current seed: %d', test_name, seed), vim.log.levels.WARN)

  local path  = vim.fn.expand(vim.fn.expand('%:p:h')) .. '/' .. test_name
  local bufnr = set_files_and_buf(path, seed, max_places_to_edit, max_edits_in_insert)
  local feed  = {}                           
  local snaps = ''

  snaps = edit_buffer(bufnr, feed, 0, 0, max_places_to_edit, max_edits_in_insert, snaps)
  write_keys2file(feed, path, false)
  if gen_keys_readable_file then
    write_keys2file(feed, path, true)
  end

  local sf = io.open(path .. FILENAMES.snaps, 'w') 
  sf:write(snaps)
  sf:close()

  io.open(path .. FILENAMES.fred_output, 'w'):close() -- NOTE+TODO: this is only temporary as fred_editor_init() needs an existent file to be initialized

  vim.api.nvim_buf_call(bufnr, function()
    vim.cmd('silent! write')
  end)

  vim.cmd('bd! ' .. tostring(bufnr))
end


-- gen_test{}
-- OR
gen_test{
  gen_keys_readable_file = true,
  -- test_name = '',
  -- max_places_to_edit= ,
  -- max_edits_in_insert= ,
}
