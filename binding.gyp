{
  "targets": [{
    "target_name": "evmc",
    "sources": [
      "src/evmc.c"
    ],
    "libraries": ["-L<(module_root_dir)/libbuild/evmc/lib/loader", "-levmc-loader"],
    "include_dirs": 
    [ "evmc/include" ],
    "cflags": [
      "-fms-extensions", " -Wno-microsoft"
    ],'xcode_settings': {
          'OTHER_CFLAGS': [ '-fms-extensions' , ' -Wno-microsoft']
    }
  }]
}