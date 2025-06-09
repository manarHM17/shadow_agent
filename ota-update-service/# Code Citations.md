# Code Citations

## License: unknown
https://github.com/snehilchopra/OpenTelemetry-StarterProject/tree/8a6a68a274165c5ac65cdcca744ff10866e192f3/foodsupplier.cc

```
);
    
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout <<
```

