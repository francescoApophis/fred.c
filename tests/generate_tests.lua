
-- This script will randomly generate keys and feed them 
-- to Neovim to edit a newly created buffer; this buffer is 
-- then written to a file called 'output.txt';
-- it will also write all the keys fed to Neovim to a file 
-- called 'fred_feed.txt' in ascii-integer form.
--
-- After feeding a key to Neovim, the script will append the current 
-- contents of the just-edited buffer to a file called 'screenshot.txt'.
-- (TODO: not yet implemented)
--
-- Then 'tests/test.c' will be able to use the keys in 
-- 'fred_feed.txt' to edit its own file (let's call it 'freddie.txt'). 
-- 'tests/test.c' will now give you the choice to do a:
--
--  - QUICK test: just compare the final 'freddie.txt' against the correct 'output.txt';
--    this will only tell you whether or not they differ without giving 
--    you much explanation; 
--
--  - SLOW test: compare the state of 'freddie.txt' against each 'screenshot'
--    in 'screenshot.txt' as each key gets consumed; this will tell you after what key, 
--    what piece and stuff failed the test
-- 
-- The script generates these files in their own separate folder:
--  tests/
--    your_test_folder_name/
--     |- fred_feed.txt
--     |- output.txt
--     |- screenshot.txt
--     |- nvim_feed.txt (optional, if you want to see the nvim-string-representation keys fed to Neovim)
-- 



-- TODO: use and save a seed for random() before generating the tests, so that tests can
-- be recreated if lost. maybe write the seed somewhere

local fmt = string.format
local rand = math.random
local rand_bool = function() return ((rand(1,2) - 1) == 1 and true) or false end
local _print = function(i) print((type(i) ~= 'table' and i) or vim.inspect(i)) end

local ESC = vim.api.nvim_replace_termcodes("<Esc>", true, false, true)
local BACKSPACE = vim.api.nvim_replace_termcodes("<BS>", true, false, true)
local NEWLINE = vim.api.nvim_replace_termcodes("\n", true, false, true)
local TAB = vim.api.nvim_replace_termcodes("\t", true, false, true)

local fred_controls = {
  ESC,
  "i",
  "h",
  "j",
  "k",
  "l",
}


-- Generate random keys from those in 'fred_constrols' and return two arrays of: 
--  1. neovim-string-representation of the keys, to be fed to Neovim itself;
--  2. ascii-integer-representation of the keys, to be fed to 'test/test.c'
---@return {string[] | number[]} 
local function generate_feeds()
  local function insert(nvim_feed, fred_feed, action)
    table.insert(nvim_feed, action)
    table.insert(fred_feed, (action == BACKSPACE and 127) or action:byte(1, -1))
  end

  local nvim_feed, fred_feed = {}, {}
  local prev_mode = nil 
  for i=1,200 do
    local mode = rand_bool()

    if i == 1 then 
      insert(nvim_feed, fred_feed, (mode and 'i') or ESC)
    elseif mode and not prev_mode then 
      insert(nvim_feed, fred_feed, 'i')
    elseif not mode and prev_mode then 
      insert(nvim_feed, fred_feed, ESC)
    else
      if mode then
        local maybe_delete = rand_bool() 
        for _=1,rand(70) do
          local action = (maybe_delete and BACKSPACE) or 
                         ((rand(20) % 2  == 0 and string.char(rand(32, 126))) or
                         (rand(20)  % 8  == 0 and NEWLINE) or 
                         (rand(20)  % 14 == 0 and TAB) or string.char(rand(32, 126)))
          insert(nvim_feed, fred_feed, action)
        end
      else
        local action = fred_controls[rand(3, #fred_controls)]
        for _=1,rand(30) do
          insert(nvim_feed, fred_feed, action)
        end
      end
    end
    prev_mode = mode
  end
  return {nvim_feed, fred_feed}
end


-- TODO: add flag to either accept feeds from an external generated_feeds() call or randomly generate them
---@param n    number   Amount of feeds to generate and test.
---@param echo boolean  Should print elements in the feeds as they get tested.
---@return {{} | {}} | {string[] | number[]} On SUCCESS return empty table with empty arrays, else return a table with the failing feeds
local function test_feeds(n, echo)
  n = n or 100000
  echo = echo or false
  for _=1,n do 
    local nvim_feed, fred_feed = unpack(generate_feeds())

    if #nvim_feed ~= #fred_feed then
      print(fmt('FAILED: feeds arrays don\'t have the same length: nvim = %d, fred = %d', #nvim_feed, #fred_feed))
      return {nvim_feed, fred_feed}
    end

    if #nvim_feed < 1 then
      print('FAILED: feeds are empty')
      return {nvim_feed, fred_feed}
    end

    for j=1,#fred_feed do
      local action = nil 
      if fred_feed[j]     == 127 then action = BACKSPACE
      elseif fred_feed[j] == 10  then action = NEWLINE
      elseif fred_feed[j] == 9   then action = TAB
      elseif fred_feed[j] == 27  then action = ESC 
      else action = string.char(fred_feed[j]) 
      end

      if action ~= nvim_feed[j] then
        print(fmt('FAILED: different values: nvim[%d] = %s, fred[%d] = %d', j, nvim_feed[j], j, fred_feed[j]))
        return {nvim_feed, fred_feed}
      end

      if echo then 
        print(action, fred_feed[j]) 
      end
    end
  end
  print('SUCCESS!')
  return {{}, {}}
end

---@param feed string[] | number[] From from generate_feeds()
---@return nil
local function print_feed(feed)
  for _, act in ipairs(feed) do
    if act == ESC then print('ESC')
    elseif act == BACKSPACE then print('BACKSPACE')
    elseif act == '\n' then print('NEWLINE')
    elseif act == '\t' then print('TAB')
    else print(act)
    end
  end
end


---@param feed      string[] | number[] From from generate_feeds()
---@param path      string
---@param feed_name string
---@param ext       string
---@return nil
local function dump_feed_to_file(feed, path, feed_name, ext)
  local feed_file = io.open(path .. feed_name .. ext, 'w')
  for _, action in ipairs(feed) do 
    local o = (type(action) ~= 'string' and tostring(action)) or action
    feed_file:write(action.. '\n')
  end
  feed_file:close()
end


---@param nvim_feed string[] 
---@param path      string
---@param ext       string
---@return nil
local function generate_output_file(nvim_feed, path, ext)
  if type(nvim_feed) ~= 'table' then
    error(fmt('Invalid feed. Expected nvim_feed (string-array) from generate_feeds(), got \'%s\'.', type(nvim_feed)))
  elseif nvim_feed[1] == nil then 
    -- NOTE: i know this does not check for array properly but idk, if you pass a table with key=1 just
    -- to fuck with this than it's your fault.
    error('Invalid feed. Expected nvim_feed (string-array) from generate_feeds(), got non-array table.')
  elseif type(nvim_feed[1]) ~= 'string' then
    error(fmt('Invalid feed. Expected nvim_feed (string-array) from generate_feeds(), got \'%s-array\'.', type(nvim_feed[1])))
  end

  local test_output_file = path .. 'output' .. ext
  local bufnr = vim.fn.bufadd(test_output_file)
  vim.fn.bufload(bufnr)
  vim.api.nvim_set_current_buf(bufnr)
  for _, action in ipairs(nvim_feed) do
    vim.api.nvim_feedkeys(action, "tn", false) -- it's blocking 
  end
  vim.api.nvim_feedkeys(ESC, "tn", false)
  vim.schedule(function() vim.cmd("write") end)
end 





--  Generate: 
--  tests/
--    test_folder_name/
--     |- fred_feed.txt     Integer representation of ASCII keys to feed to test.c
--     |- output.txt        Output file after fred_feed.txt has been fed to test.c
--     |- nvim_feed.txt     If *should_dump_nvim_feed* is True. For when you wanna *visually* debug 
--                          the output that test_main.c generates when it reads and translate 
--                          back to ascii the fred_feed.txt input.
--                          ATTENTION: each action in the file is followed by a '\n' , so if the action
--                          is a '\n' there will be 2 newlines. This is okay here since the DUMPED nvim_feed is not used 
--                          for anything. Just keep in mind that it's not an error
---@param test_folder_name      string
---@param should_dump_nvim_feed boolean
---@return nil
local function generate_test(test_folder_name, should_dump_nvim_feed)
  if type(test_folder_name) ~= 'string' then
    error(fmt('Invalid name for test folder: expected \'string\', got \'%s\'', type(test_folder_name)))
  end
  local should_dump_nvim_feed = should_dump_nvim_feed or false

  local path = vim.fn.expand(vim.fn.expand('%:p:h')) .. '/' .. test_folder_name .. '/'
  local ext = '.txt'
  vim.api.nvim_exec2('!mkdir -p ' .. path, {})

  local nvim_feed, fred_feed = unpack(generate_feeds())

  if should_dump_nvim_feed then 
    dump_feed_to_file(nvim_feed, path, 'nvim_feed', ext)
  end

  generate_output_file(nvim_feed, path, ext)
  dump_feed_to_file(fred_feed, path, 'fred_feed', ext)
end





-- local nvim_feed, fred_feed = unpack(test_feeds()) -- you can dump them in files if you want
-- generate_test('test_1', true)

