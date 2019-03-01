{
  "targets": [{
    "target_name": "evmc",
    "sources": [
      "src/evmc.c"
    ],
    "libraries": ["-L<(module_root_dir)/libbuild/evmc/lib/loader", "-levmc-loader"],
    "include_dirs": 
    [ "evmc/include" ]
  }]
}